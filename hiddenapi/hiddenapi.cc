/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <iostream>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/stringpiece.h"
#include "base/unix_file/fd_file.h"
#include "base/time_utils.h"
#include "dex_file-inl.h"
#include "dex_file_loader.h"
#include "leb128.h"
#include "mem_map.h"
#include "os.h"
#include "runtime.h"

// This tool iterates over all class members inside given DEX files
// and modifies their access flags if their signatures appear on one
// of two lists - greylist and blacklist - provided as text file inputs.
// These access flags denote to the runtime that the marked methods/fields
// should be treated as internal APIs with restricted access.
//
// Two bits of information are encoded in the DEX access flags. These
// are encoded as unsigned LEB128 values in DEX and so as to not
// increase the size of the DEX, different modifiers were chosen to
// carry the information under different circumstances.
//
// First bit is encoded as the inversion of visibility access flags
// (bits 2:0). At most one of these flags can be set at any given time.
// Inverting these bits therefore produces a value where at least two
// bits are set and there is never any loss of information.
//
// Second bit is encoded differently for each given type of class
// member as there is no single unused bit such that setting it would
// not increase the size of the LEB128 encoding.
//  - Bit 5 for fields as it carries no other meaning
//  - Bit 5 for non-native methods, as `synchronized` can only be set
//    on native methods
//  - Bit 10 for native methods, as it carries no meaning and bit 8
//    (native) will make the LEB128 encoding at least two bytes long

namespace art {

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < original_argc; ++i) {
    command.push_back(original_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void Usage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("Usage: hiddenapi [options]...");
  UsageError("");

  exit(EXIT_FAILURE);
}

class DexClass {
 public:
  DexClass(const DexFile &dex_file, uint32_t idx)
    : dex_file_(dex_file),
      class_def_(const_cast<DexFile::ClassDef&>(dex_file.GetClassDef(idx))) {}

  const dex::TypeIndex GetClassIndex() const { return class_def_.class_idx_; }

  const uint8_t *GetData() const { return dex_file_.GetClassData(class_def_); }

  const char *GetDescriptor() const { return dex_file_.GetClassDescriptor(class_def_); }

  const DexFile &GetDexFile() const { return dex_file_; }

 private:
  const DexFile &dex_file_;
  DexFile::ClassDef &class_def_;
};

class DexMember {
 public:
  DexMember(const DexClass &klass, const ClassDataItemIterator &it)
      : klass_(klass), it_(it) {
    DCHECK_EQ(it_.IsAtMethod() ? GetMethodId().class_idx_ : GetFieldId().class_idx_,
              klass_.GetClassIndex());
  }

  // Returns true if hidden flags can be extracted from raw access flags of the member.
  // This can mean either that the DEX file already had hidden flags stored in it, or
  // that the DEX file is using access flag bits which it should not.
  bool UsesHiddenBits() const {
    return it_.GetRawMemberAccessFlags() != it_.GetRawMemberAccessFlagsWithHiddenBits();
  }

  // Sets hidden bits in access flags and writes them back into the DEX memory representation.
  // Note that this will not update the cached data of ClassDataItemIterator until it iterates
  // over this item again and therefore will fail a DCHECK if it is called multiple times on
  // the same iterator object.
  template<bool bit1, bool bit2>
  void SetHidden() {
    // Get access flags stripped of hidden flags.
    uint32_t new_access_flags = it_.GetRawMemberAccessFlags();

    if (bit1) {
      // Set first bit by flipping the visibility flags.
      new_access_flags ^= kAccVisibilityFlags;
    }

    if (bit2) {
      new_access_flags |= it_.GetMemberHiddenBit();
    }

    // Locate the LEB128-encoded access flags in class data.
    const uint8_t *ptr = it_.MemberPointer();
    DecodeUnsignedLeb128(&ptr);  // member index diff
    uint8_t *access_flags_ptr = const_cast<uint8_t*>(ptr);

    // Try to decode and make sure these match the access flags cached in ClassDataItemIterator.
    if (kIsDebugBuild) {
      uint32_t decoded_access_flags = DecodeUnsignedLeb128(&ptr);
      uint32_t cached_access_flags = it_.GetRawMemberAccessFlagsWithHiddenBits();
      DCHECK_EQ(cached_access_flags, decoded_access_flags);

      // Check that we do not need to increase the size of the DEX.
      DCHECK_EQ(UnsignedLeb128Size(decoded_access_flags), UnsignedLeb128Size(new_access_flags));
    }

    // Write `new_access_flags` into the class data.
    uint8_t *end_ptr = EncodeUnsignedLeb128(access_flags_ptr, new_access_flags);
    DCHECK_EQ(ptr, end_ptr);
  }

  // Returns true if this member's API entry is in `list`.
  bool IsOnApiList(const std::unordered_set<std::string> &list) const {
    return list.find(GetApiEntry()) != list.end();
  }

  // Construct a string with a unique signature of this class member.
  std::string GetApiEntry() const {
    std::stringstream ss;
    ss << klass_.GetDescriptor() << "->";
    if (it_.IsAtMethod()) {
      const DexFile::MethodId &mid = GetMethodId();
      ss << klass_.GetDexFile().GetMethodName(mid)
         << klass_.GetDexFile().GetMethodSignature(mid).ToString();
    } else {
      const DexFile::FieldId &fid = GetFieldId();
      ss << klass_.GetDexFile().GetFieldName(fid) << ":"
         << klass_.GetDexFile().GetFieldTypeDescriptor(fid);
    }
    return ss.str();
  }

 private:
  const DexFile::MethodId &GetMethodId() const {
    DCHECK(it_.IsAtMethod());
    return klass_.GetDexFile().GetMethodId(it_.GetMemberIndex());
  }

  const DexFile::FieldId &GetFieldId() const {
    DCHECK(!it_.IsAtMethod());
    return klass_.GetDexFile().GetFieldId(it_.GetMemberIndex());
  }

  const DexClass &klass_;
  const ClassDataItemIterator &it_;
};

class HiddenApi FINAL {
 public:
  HiddenApi()
      : check_flags_not_set_(true), print_hidden_api_(false) {}

  ~HiddenApi() {
    LogCompletionTime();
  }

  void ParseArgs(int argc, char **argv) {
    original_argc = argc;
    original_argv = argv;

    InitLogging(argv, Runtime::Abort);

    // Skip over the command name.
    argv++;
    argc--;

    if (argc == 0) {
      Usage("No arguments specified");
    }

    for (int i = 0; i < argc; ++i) {
      const StringPiece option(argv[i]);
      const bool log_options = false;
      if (log_options) {
        LOG(INFO) << "hiddenapi: option[" << i << "]=" << argv[i];
      }
      if (option == "--no-flags-check") {
        check_flags_not_set_ = false;
      } else if (option == "--print-hidden-api") {
        print_hidden_api_ = true;
      } else if (option.starts_with("--dex=")) {
        dex_paths_.push_back(option.substr(strlen("--dex=")).ToString());
      } else if (option.starts_with("--greylist=")) {
        greylist_path_ = option.substr(strlen("--greylist=")).ToString();
      } else if (option.starts_with("--blacklist=")) {
        blacklist_path_ = option.substr(strlen("--blacklist=")).ToString();
      } else {
        Usage("Unknown argument '%s'", option.data());
      }
    }
  }

  int ProcessDexFiles() {
    if (dex_paths_.empty()) {
      Usage("No DEX files specified");
    }

    if (greylist_path_.empty() && blacklist_path_.empty()) {
      Usage("No API file specified");
    }

    if (!OpenApiFiles()) {
      return -1;
    }

    MemMap::Init();
    if (!OpenDexFiles()) {
      return -1;
    }

    DCHECK(!dex_files_.empty());
    for (auto &dex_file : dex_files_) {
      CategorizeAllClasses(*dex_file.get());
    }

    UpdateDexChecksums();

    return 0;
  }

 private:
  void LogCompletionTime() {
    static constexpr uint64_t kLogThresholdTime = MsToNs(100);  // 100ms
    uint64_t time_taken = NanoTime() - start_ns_;
    if (time_taken > kLogThresholdTime) {
      LOG(WARNING) << "hiddenapi took " << PrettyDuration(time_taken);
    }
  }

  bool OpenApiFiles() {
    for (auto pair : { std::make_pair(greylist_path_, &greylist_),
                       std::make_pair(blacklist_path_, &blacklist_) }) {
      DCHECK(pair.second->empty());
      if (pair.first.empty()) {
        continue;
      }

      std::ifstream api_file(pair.first, std::ifstream::in);
      if (api_file.fail()) {
        LOG(ERROR) << "Open failed for '" << pair.first << "' " << strerror(errno);
        return false;
      }

      for (std::string line; std::getline(api_file, line);) {
        pair.second->insert(line);
      }

      api_file.close();
    }

    return true;
  }

  bool OpenDexFiles() {
    DCHECK(dex_files_.empty());

    for (const std::string& filename : dex_paths_) {
      std::string error_msg;

      File fd(filename.c_str(), O_RDWR, /* check_usage */ false);
      if (fd.Fd() == -1) {
        LOG(ERROR) << "Unable to open file '" << filename << "': " << strerror(errno);
        return false;
      }

      std::unique_ptr<const DexFile> dex_file(DexFileLoader::OpenDex(fd.Release(),
                                                                     /* location */ filename,
                                                                     /* verify */ false,
                                                                     /* verify_checksum */ true,
                                                                     /* mmap_shared */ true,
                                                                     &error_msg));
      if (dex_file.get() == nullptr) {
        LOG(ERROR) << "Open failed for '" << filename << "' " << error_msg;
        return false;
      } else if (!dex_file->IsStandardDexFile()) {
        LOG(ERROR) << "Expected a standard dex file '" << filename << "'";
        return false;
      } else if (!dex_file->EnableWrite()) {
        LOG(ERROR) << "Failed to enable write permission for '" << filename << "'";
        return false;
      } else {
        dex_files_.push_back(std::move(dex_file));
      }
    }
    return true;
  }

  void CategorizeAllClasses(const DexFile &dex_file) {
    for (uint32_t class_idx = 0; class_idx < dex_file.NumClassDefs(); ++class_idx) {
      DexClass klass(dex_file, class_idx);
      const uint8_t *klass_data = klass.GetData();
      if (klass_data == nullptr) {
        continue;
      }

      for (ClassDataItemIterator it(klass.GetDexFile(), klass_data); it.HasNext(); it.Next()) {
        DexMember member(klass, it);
        if (check_flags_not_set_) {
          CHECK(!member.UsesHiddenBits())
              << "Member \"" << member.GetApiEntry() << "\" is using the hidden accesss flags.";
        }
        bool member_hidden = false;
        if (member.IsOnApiList(blacklist_)) {
          member.SetHidden<true, true>();
          member_hidden = true;
        } else if (member.IsOnApiList(greylist_)) {
          member.SetHidden<true, false>();
          member_hidden = true;
        } else {
          member.SetHidden<false, false>();
          member_hidden = false;
        }
        if (print_hidden_api_ && member_hidden) {
          std::cout << member.GetApiEntry() << std::endl;
        }
      }
    }
  }

  void UpdateDexChecksums() {
    for (auto &dex_file : dex_files_) {
      DexFile::Header *header = const_cast<DexFile::Header*>(&dex_file->GetHeader());
      header->checksum_ = dex_file->CalculateChecksum();
    }
  }

  // Test whether access flags already have hidden bits set and fail if they do.
  bool check_flags_not_set_;

  // Print signatures of APIs which have been grey-/blacklisted.
  bool print_hidden_api_;

  // Paths to DEX files which should be processed.
  std::vector<std::string> dex_paths_;

  // Paths to text files which contain the grey- and blacklists of API members.
  std::string greylist_path_;
  std::string blacklist_path_;

  // Opened DEX files. Note that these are opened as `const` but eventually will be written into.
  std::vector<std::unique_ptr<const DexFile>> dex_files_;

  // Signatures of DEX members loaded from `greylist_path`, `blacklist_path`.
  std::unordered_set<std::string> greylist_;
  std::unordered_set<std::string> blacklist_;

  uint64_t start_ns_;
};

}  // namespace art

int main(int argc, char **argv) {
  art::HiddenApi hiddenapi;

  // Parse arguments. Argument mistakes will lead to exit(EXIT_FAILURE) in UsageError.
  hiddenapi.ParseArgs(argc, argv);
  return hiddenapi.ProcessDexFiles();
}
