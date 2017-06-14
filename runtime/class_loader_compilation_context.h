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

#ifndef ART_RUNTIME_CLASS_LOADER_COMPILATION_CONTEXT_H_
#define ART_RUNTIME_CLASS_LOADER_COMPILATION_CONTEXT_H_

#include <string>
#include <vector>

#include "base/dchecked_vector.h"
#include "dex_file.h"
#include "oat_file.h"
#include "jni.h"

namespace art {

// Utility class which hold the class loader context used during compilation.
class ClassLoaderCompilationContext {
 public:
  enum ClassLoaderType {
    kPathClassLoader = 0,
    kDelegateLastClassLoader = 1
  };

  struct ClassLoaderInfo {
    // The type of this class loader.
    ClassLoaderType type;
    // The list of class path elements that this loader loads.
    // Note that this list may contain relative paths.
    std::vector<std::string> classpath;
    // After OpenDexFiles is called this holds the opened dex files.
    std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
    // After OpenDexFiles, in case some of the dex files were opened from their oat files
    // this holds the list of opened oat files.
    std::vector<std::unique_ptr<OatFile>> opened_oat_files;

    explicit ClassLoaderInfo(ClassLoaderType cl_type) : type(cl_type) {}
  };

  ClassLoaderCompilationContext();

  // Opens requested class path files and appends them to opened_dex_files. If the dex files have
  // been stripped, this opens them from their oat files and appends them to opened_oat_files.
  void OpenDexFiles(InstructionSet isa, std::string& classpath_dir);

  // Validates the uniqueness of the dex elements present in the class loader chain
  // relative tot the specified compilation_sources.
  bool ValidateUniquenessOfElements(const dchecked_vector<std::string>& compilation_sources);

  // Creates the entire class loader hierarchy according to the current context.
  jobject CreateClassLoader(std::vector<const DexFile*>& compilation_sources);

  // Encodes the context as a string suitable to be added in oat files.
  // (so that it can be read and verified at runtime against the actual class
  // loader hierarchy).
  std::string EncodeContextForOatFile(const std::string& base_dir);

  void FlattenDexFiles(std::vector<const DexFile*>* result);

  // Creates the class loader context from the given string.
  // The format: ClassLoaderType1[ClasspathElem1:ClasspathElem2...];ClassLoaderType2[...]...
  // ClassLoaderType is either "PCL" (PathClassLoader) or "DLC" (DelegateLastClassLoader).
  // ClasspathElem is the path of dex/jar/apk file.
  static std::unique_ptr<ClassLoaderCompilationContext> Create(const std::string& spec);

 private:
  bool Parse(const std::string& spec);

  // The class loader chain represented as a vector.
  // The parent of class_loader_chain_[i] is class_loader_chain_[i++].
  // The parent of the last element if assumed to be the boot class loader.
  std::vector<ClassLoaderInfo> class_loader_chain_;

  // Whether or not the class loader context should be ignored at runtime when loading the oat files.
  // When true, dex2oat will use OatFile::kSpecialSharedLibrary as the classpath key in the oat file.
  // TODO(calin): Can we get rid of this and cover all relevant use cases? (e.g. APS)
  bool special_shared_library_;
};

}  // namespace art
#endif  // ART_RUNTIME_CLASS_LOADER_COMPILATION_CONTEXT_H_
