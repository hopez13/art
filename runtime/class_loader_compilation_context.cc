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

  // At this point we know the format is ok; continue and extract the classpath.
  size_t type_str_size = class_loader_type_str.length();
  std::string classpath = class_loader_spec.substr(type_str_size + 1,
                                                   class_loader_spec.length() - type_str_size - 2);

  // Note that we allowed class loaders with an empty class path in order to support a custom
  // class loader for the source dex files.

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
// been stripped, this opens them from their oat files (which get added to opened_oat_files).
bool ClassLoaderCompilationContext::OpenDexFiles(InstructionSet isa,
                                                 const std::string& classpath_dir) {
  CHECK(!dex_files_open_attempted_)<< "OpenDexFiles should not be called twice";

  dex_files_open_attempted_ = true;
  // Assume we can open all dex files. If not, we will set this to false as we
  // go.
  dex_files_open_result_ = true;

  if (special_shared_library_) {
    // Nothing to open if the context is a special shared library.
    return true;
  }

  // Note that we try to open all dex files even if some fails.
  // We may get resource-only apks which we cannot load.
  // TODO(calin): Refine the dex opening interface to be able to tell if an archive contains
  // no dex files. So that we can distinguish the real failures...
  for (ClassLoaderInfo& info : class_loader_chain_) {
    for (const std::string& cp_elem : info.classpath) {
      // If path is relative, append it to the provided base directory.
      std::string location = cp_elem;
      if (location[0] != '/') {
        location = classpath_dir + '/' + location;
      }
      static constexpr bool kVerifyChecksum = true;
      std::string error_msg;
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
        std::vector<std::unique_ptr<const DexFile>> oat_dex_files;
        if (oat_file != nullptr &&
            OatFileAssistant::LoadDexFiles(*oat_file, location, &oat_dex_files)) {
          info.opened_oat_files.push_back(std::move(oat_file));
          info.opened_dex_files.insert(info.opened_dex_files.end(),
                                       std::make_move_iterator(oat_dex_files.begin()),
                                       std::make_move_iterator(oat_dex_files.end()));
        } else {
          LOG(ERROR) << "Could not open dex files from location: " << location;
          dex_files_open_result_ = false;
        }
      }
    }
  }

  return dex_files_open_result_;
}

bool ClassLoaderCompilationContext::RemoveLocationsFromClasspaths(
    const dchecked_vector<std::string>& locations) {
  CHECK(!dex_files_open_attempted_)
      << "RemoveLocationsFromClasspaths cannot be call after OpenDexFiles";

  std::set<std::string> canonical_locations;
  for (const std::string& location : locations) {
    canonical_locations.insert(DexFile::GetDexCanonicalLocation(location.c_str()));
  }
  bool removed_locations = false;
  for (ClassLoaderInfo& info : class_loader_chain_) {
    size_t initial_size = info.classpath.size();
    auto kept_it = std::remove_if(
        info.classpath.begin(),
        info.classpath.end(),
        [canonical_locations](const std::string& location) {
            return ContainsElement(canonical_locations,
                                   DexFile::GetDexCanonicalLocation(location.c_str()));
        });
    info.classpath.erase(kept_it, info.classpath.end());
    if (initial_size != info.classpath.size()) {
      removed_locations = true;
    }
  }
  return removed_locations;
}

std::string ClassLoaderCompilationContext::EncodeContextForOatFile(
      const std::string& base_dir) const {
  CheckDexFilesOpened("EncodeContextForOatFile");

  std::ostringstream out;

  if (special_shared_library_) {
    return OatFile::kSpecialSharedLibrary;
  }

  if (class_loader_chain_.empty()) {
    return "";
  }

  // TODO(calin): Transition period: assume we only have a classloader until
  // the oat file assistant implements the full class loaders check.
  CHECK_EQ(1u, class_loader_chain_.size());

  return OatFile::EncodeDexFileDependencies(MakeNonOwningPointerVector(
      class_loader_chain_[0].opened_dex_files), base_dir);
}

jobject ClassLoaderCompilationContext::CreateClassLoader(
    std::vector<const DexFile*>& compilation_sources) const {
  CheckDexFilesOpened("CreateClassLoader");

  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  std::vector<const DexFile*> class_path_files;

  // TODO(calin): Transition period: assume we only have a classloader until
  // the oat file assistant implements the full class loaders check.
  if (!class_loader_chain_.empty()) {
    CHECK_EQ(1u, class_loader_chain_.size());
    CHECK_EQ(kPathClassLoader, class_loader_chain_[0].type);
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

std::vector<const DexFile*> ClassLoaderCompilationContext::FlattenOpenedDexFiles() const {
  CheckDexFilesOpened("FlattenOpenedDexFiles");

  std::vector<const DexFile*> result;
  for (const ClassLoaderInfo& info : class_loader_chain_) {
    for (const std::unique_ptr<const DexFile>& dex_file : info.opened_dex_files) {
      result.push_back(dex_file.get());
    }
  }
  return result;
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

void ClassLoaderCompilationContext::CheckDexFilesOpened(const std::string& calling_method) const {
  CHECK(dex_files_open_attempted_)
      << "Dex files were not successfully opened before the call to " << calling_method
      << "attempt=" << dex_files_open_attempted_ << ", result=" << dex_files_open_result_;
}
}  // namespace art

