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

#include "class_loader_compilation_context.h"

#include "base/dchecked_vector.h"
#include "base/stl_util.h"
#include "class_linker.h"
#include "dex_file.h"
#include "oat_file_assistant.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {

static const std::string kPathClassLoaderType = "PCL";
static const std::string kDelegateLastClassLoaderType = "DLC";
static constexpr char kClassLoaderOpeningMark = '[';
static constexpr char kClassLoaderClosingMark = ']';
static constexpr char kClassLoaderSep = ';';
static constexpr char kClasspathSep = ':';

ClassLoaderCompilationContext::ClassLoaderCompilationContext()
    : special_shared_library_(false),
      dex_files_open_attempted_(false),
      dex_files_open_result_(false) {}

std::unique_ptr<ClassLoaderCompilationContext> ClassLoaderCompilationContext::Create(
    const std::string& spec) {
  std::unique_ptr<ClassLoaderCompilationContext> result(new ClassLoaderCompilationContext());
  if (result->Parse(spec)) {
    return result;
  } else {
    return nullptr;
  }
}

// The expected format is: "ClassLoaderType1[ClasspathElem1:ClasspathElem2...]".
bool ClassLoaderCompilationContext::ParseClassLoaderSpec(
    const std::string& class_loader_spec,
    ClassLoaderType class_loader_type) {
  std::string class_loader_type_str = GetClassLoaderTypeName(class_loader_type);
  if (class_loader_spec.compare(0, class_loader_type_str.length(), class_loader_type_str) != 0) {
    // Not the class loader we are looking for.
    return false;
  }
  // Check the opening and closing markers.
  if (class_loader_spec[class_loader_type_str.length()] != kClassLoaderOpeningMark) {
    return false;
  }
  if (class_loader_spec[class_loader_spec.length() - 1] != kClassLoaderClosingMark) {
    return false;
  }

  // Check the classpath (obtained by removing the type and the opening/closing markers)
  size_t type_str_size = class_loader_type_str.length();
  std::string classpath = class_loader_spec.substr(type_str_size + 1,
                                                   class_loader_spec.length() - type_str_size - 2);
  if (classpath.empty()) {
    // Empty class paths are not allowed.
    return false;
  }
  // At this point we know the format is ok; continue and extract the classpath.
  class_loader_chain_.push_back(ClassLoaderInfo(class_loader_type));
  Split(classpath, kClasspathSep, &class_loader_chain_.back().classpath);

  return true;
}

// The format: ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]...
// ClassLoaderType is either "PCL" (PathClassLoader) or "DLC" (DelegateLastClassLoader).
// ClasspathElem is the path of dex/jar/apk file.
bool ClassLoaderCompilationContext::Parse(const std::string& spec) {
  // Stop early if we detect the special shared library, which may be passed as the classpath
  // for dex2oat when we want to skip the shared libraries check.
  if (spec == OatFile::kSpecialSharedLibrary) {
    special_shared_library_ = true;
    return true;
  }
  std::vector<std::string> class_loader_elems;
  Split(spec, kClassLoaderSep, &class_loader_elems);
  const ClassLoaderType kValidTypes[] = {kPathClassLoader, kDelegateLastClassLoader};
  for (const std::string& elem : class_loader_elems) {
    bool valid_elem = false;
    for (ClassLoaderType type : kValidTypes) {
      if (ParseClassLoaderSpec(elem, type)) {
        valid_elem = true;
        break;
      }
    }
    if (!valid_elem) {
      LOG(ERROR) << "Invalid class loader spec: " << elem;
      return false;
    }
  }
  return true;
}

// Opens requested class path files and appends them to opened_dex_files. If the dex files have
// been stripped, this opens them from their oat files and appends them to opened_oat_files.
bool ClassLoaderCompilationContext::OpenDexFiles(InstructionSet isa,
                                                 const std::string& classpath_dir) {
  if (dex_files_open_attempted_) {
    return dex_files_open_result_;
  }
  dex_files_open_attempted_ = true;

  if (special_shared_library_) {
    // Nothing to open if the context is a special shared library.
    dex_files_open_result_ = true;
    return true;
  }

  for (ClassLoaderInfo& info : class_loader_chain_) {
    for (const std::string& cp_elem : info.classpath) {
      // If path is relative, append it to the provided base directory.
      std::string location = cp_elem;
      if (location[0] != '/') {
        location = classpath_dir + '/' + location;
      }
      static constexpr bool kVerifyChecksum = true;
      std::string error_msg;
      LOG(ERROR) << " CALIN TRYING " << location;
      if (!DexFile::Open(location.c_str(),
                         location.c_str(),
                         kVerifyChecksum,
                         &error_msg,
                         &info.opened_dex_files)) {
        // If we fail to open the dex file because it's been stripped, try to open the dex file
        // from its corresponding oat file.
        // This could happen when we need to recompile a pre-build whose dex code has bee striped.
        // (for example, if the pre-build is only quicken and we want to re-compile it
        // speed-profile).
        OatFileAssistant oat_file_assistant(location.c_str(), isa, false);
        std::unique_ptr<OatFile> oat_file(oat_file_assistant.GetBestOatFile());
        if (oat_file == nullptr) {
          LOG(WARNING) << "Failed to open dex file and associated oat file for '" << location
                       << "': " << error_msg;
          dex_files_open_result_ = false;
          return false;
        } else {
          std::vector<std::unique_ptr<const DexFile>> oat_dex_files;
          if (!OatFileAssistant::LoadDexFiles(*oat_file, location, &oat_dex_files)) {
            dex_files_open_result_ = false;
            return false;
          }
          LOG(ERROR) << " CALIN " << oat_dex_files.size();
          info.opened_oat_files.push_back(std::move(oat_file));
          info.opened_dex_files.insert(info.opened_dex_files.end(),
                                       std::make_move_iterator(oat_dex_files.begin()),
                                       std::make_move_iterator(oat_dex_files.end()));
        }
      } else {
        LOG(ERROR) << " CALIN OK " << location;
      }
    }
  }
  dex_files_open_result_ = true;
  return true;
}

bool ClassLoaderCompilationContext::ValidateUniquenessOfElements(
    const dchecked_vector<std::string>& compilation_sources) const {
  std::set<std::string> canonical_locations;
  for (const std::string& location : compilation_sources) {
    canonical_locations.insert(DexFile::GetDexCanonicalLocation(location.c_str()));
  }
  for (const ClassLoaderInfo& info : class_loader_chain_) {
    for (const std::string &location : info.classpath) {
      bool fresh = canonical_locations.insert(
          DexFile::GetDexCanonicalLocation(location.c_str())).second;
      if (!fresh) {
        // TODO(calin): Discuss: this was originally filtering out the source dex files
        // from the classpath. Is to harsh to reject any duplicates?
        return false;
      }
    }
  }
  return true;
}

// TODO(calin): Transition period: assume we only have a classloader until
// the oat file assistant implements the full class loaders check.
std::string ClassLoaderCompilationContext::EncodeContextForOatFile(
      const std::string& base_dir) const {
  std::ostringstream out;

  if (special_shared_library_) {
    return OatFile::kSpecialSharedLibrary;
  }

  if (class_loader_chain_.empty()) {
    return "";
  }
  CHECK_EQ(1u, class_loader_chain_.size());

  return OatFile::EncodeDexFileDependencies(MakeNonOwningPointerVector(
      class_loader_chain_[0].opened_dex_files), base_dir);
}

// TODO(calin): Transition period: assume we only have a classloader until
// the oat file assistant implements the full class loaders check.
jobject ClassLoaderCompilationContext::CreateClassLoader(
    std::vector<const DexFile*>& compilation_sources) const {
  if (!dex_files_open_attempted_ || !dex_files_open_result_) {
    return nullptr;
  }

  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  std::vector<const DexFile*> class_path_files;

  if (!class_loader_chain_.empty()) {
    CHECK_EQ(1u, class_loader_chain_.size());
    class_path_files = MakeNonOwningPointerVector(class_loader_chain_[0].opened_dex_files);
  }

  // Classpath: first the class-path given; then the dex files we'll compile.
  // Thus we'll resolve the class-path first.
  class_path_files.insert(class_path_files.end(),
                          compilation_sources.begin(),
                          compilation_sources.end());

  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  return class_linker->CreatePathClassLoader(self, class_path_files);
}
bool ClassLoaderCompilationContext::FlattenOpenedDexFiles(
      std::vector<const DexFile*>* result) const {
  if (!dex_files_open_attempted_ || !dex_files_open_result_) {
    return false;
  }
  for (const ClassLoaderInfo& info : class_loader_chain_) {
    for (const std::unique_ptr<const DexFile>& dex_file : info.opened_dex_files) {
      result->push_back(dex_file.get());
    }
  }
  return true;
}

std::string ClassLoaderCompilationContext::GetClassLoaderTypeName(ClassLoaderType type) const {
  switch (type) {
    case kPathClassLoader: return kPathClassLoaderType;
    case kDelegateLastClassLoader: return kDelegateLastClassLoaderType;
    default:
      LOG(FATAL) << "Invalid class loader type " << type;
      UNREACHABLE();
  }
}

}  // namespace art

