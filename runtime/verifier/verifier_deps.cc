/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "verifier_deps.h"

#include "compiler_callbacks.h"
#include "mirror/class-inl.h"

namespace art {
namespace verifier {

VerifierDeps::VerifierDeps()
    : lock_(kLockDescription, kVerifierDepsLock) {}

void VerifierDeps::AddCompiledDexFile(const DexFile& dex_file) {
  MutexLock mu(Thread::Current(), lock_);
  dex_deps_[&dex_file];  // Construct DexFileDeps struct for `dex_file` if not present already.
}

template <typename T>
uint16_t VerifierDeps::GetAccessFlags(T* element) {
  static_assert(kAccJavaFlagsMask == 0xFFFF, "Unexpected value of a constant");
  if (element == nullptr) {
    return VerifierDeps::kUnresolvedMarker;
  } else {
    uint16_t access_flags = Low16Bits(element->GetAccessFlags());
    CHECK_NE(access_flags, VerifierDeps::kUnresolvedMarker);
    return access_flags;
  }
}

template <typename T>
uint32_t VerifierDeps::GetDeclaringClassStringId(const DexFile& dex_file, T* element) {
  static_assert(kAccJavaFlagsMask == 0xFFFF, "Unexpected value of a constant");
  if (element == nullptr) {
    return VerifierDeps::kUnresolvedMarker;
  } else {
    std::string temp;
    uint32_t string_id = GetIdFromString(
        dex_file, element->GetDeclaringClass()->GetDescriptor(&temp));
    return string_id;
  }
}

uint32_t VerifierDeps::GetIdFromString(const DexFile& dex_file, const std::string& str) {
  const DexFile::StringId* string_id = dex_file.FindStringId(str.c_str());
  if (string_id != nullptr) {
    // String is in the DEX file. Return its ID.
    return dex_file.GetIndexForStringId(*string_id);
  }

  // String is not in the DEX file. Assign a new ID to it which is higher than
  // the number of strings in the DEX file.

  DexFileDeps* deps = GetDexFileDeps(dex_file);
  DCHECK(deps != nullptr);

  uint32_t num_ids_in_dex = dex_file.NumStringIds();
  uint32_t num_extra_ids = deps->strings_.size();

  for (size_t i = 0; i < num_extra_ids; ++i) {
    if (deps->strings_[i] == str) {
      return num_ids_in_dex + i;
    }
  }

  deps->strings_.push_back(str);

  uint32_t new_id = num_ids_in_dex + num_extra_ids;
  CHECK_GE(new_id, num_ids_in_dex);  // check for overflows
  DCHECK_EQ(str, GetStringFromId(dex_file, new_id));

  return new_id;
}

std::string VerifierDeps::GetStringFromId(const DexFile& dex_file, uint32_t string_id) {
  uint32_t num_ids_in_dex = dex_file.NumStringIds();
  if (string_id < num_ids_in_dex) {
    return std::string(dex_file.StringDataByIdx(string_id));
  } else {
    DexFileDeps* deps = GetDexFileDeps(dex_file);
    DCHECK(deps != nullptr);
    string_id -= num_ids_in_dex;
    CHECK_LT(string_id, deps->strings_.size());
    return deps->strings_[string_id];
  }
}

VerifierDeps::DexFileDeps* VerifierDeps::GetDexFileDeps(const DexFile& dex_file) {
  auto it = dex_deps_.find(&dex_file);
  return (it == dex_deps_.end()) ? nullptr : &(it->second);
}

bool VerifierDeps::IsInClassPath(mirror::Class* klass) {
  DCHECK(klass != nullptr);

  mirror::DexCache* dex_cache = klass->GetDexCache();
  if (dex_cache == nullptr) {
    // This is a synthesized class, e.g. an array.
    return true;
  }

  const DexFile* dex_file = dex_cache->GetDexFile();
  DCHECK(dex_file != nullptr);

  // Test if the `dex_deps_` contains an entry for `dex_file`. If not, the dex
  // file was not registered as being compiled and we assume `klass` is in the
  // class path.
  return (GetDexFileDeps(*dex_file) == nullptr);
}

void VerifierDeps::AddClassResolution(const DexFile& dex_file,
                                      uint16_t type_idx,
                                      mirror::Class* klass) {
  MutexLock mu(Thread::Current(), lock_);

  if (klass != nullptr && !IsInClassPath(klass)) {
    // Class resolved into one of the DEX files which are being compiled.
    // This is not a class path dependency.
    return;
  }

  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  dex_deps->classes_.emplace(ClassResolution(type_idx, GetAccessFlags(klass)));
}

void VerifierDeps::AddFieldResolution(const DexFile& dex_file,
                                      uint32_t field_idx,
                                      ArtField* field) {
  MutexLock mu(Thread::Current(), lock_);

  if (field != nullptr && !IsInClassPath(field->GetDeclaringClass())) {
    // Field resolved into one of the DEX files which are being compiled.
    // This is not a class path dependency.
    return;
  }

  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  dex_deps->fields_.emplace(FieldResolution(
      field_idx, GetAccessFlags(field), GetDeclaringClassStringId(dex_file, field)));
}

void VerifierDeps::AddMethodResolution(const DexFile& dex_file,
                                       uint32_t method_idx,
                                       MethodResolutionType resolution_type,
                                       ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);

  if (method != nullptr && !IsInClassPath(method->GetDeclaringClass())) {
    // Method resolved into one of the DEX files which are being compiled.
    // This is not a class path dependency.
    return;
  }

  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  MethodResolution method_tuple(method_idx,
                                GetAccessFlags(method),
                                GetDeclaringClassStringId(dex_file, method));
  if (resolution_type == kDirectMethodResolution) {
    dex_deps->direct_methods_.emplace(method_tuple);
  } else if (resolution_type == kVirtualMethodResolution) {
    dex_deps->virtual_methods_.emplace(method_tuple);
  } else {
    DCHECK_EQ(resolution_type, kInterfaceMethodResolution);
    dex_deps->interface_methods_.emplace(method_tuple);
  }
}

void VerifierDeps::AddAssignability(const DexFile& dex_file,
                                    mirror::Class* destination,
                                    mirror::Class* source,
                                    bool is_strict,
                                    bool is_assignable) {
  DCHECK(destination != nullptr && destination->IsResolved() && !destination->IsPrimitive());
  DCHECK(source != nullptr && source->IsResolved() && !source->IsPrimitive());

  if (destination == source ||
      destination->IsObjectClass() ||
      (!is_strict && destination->IsInterface())) {
    // Cases when `destination` is trivially assignable from `source`.
    DCHECK(is_assignable);
    return;
  }

  if (destination->IsArrayClass() != source->IsArrayClass()) {
    // One is an array, the other one isn't and `destination` is not Object.
    // This is not a dependency because assignability will always trivially fail.
    DCHECK(!is_assignable);
    return;
  }

  if (destination->IsArrayClass()) {
    // Both types are arrays. Add recursively.
    DCHECK(source->IsArrayClass());
    AddAssignability(dex_file,
                     destination->GetComponentType(),
                     source->GetComponentType(),
                     /* is_strict */ true,
                     is_assignable);
    return;
  }

  DCHECK_EQ(is_assignable, destination->IsAssignableFrom(source));

  MutexLock mu(Thread::Current(), lock_);

  if (!IsInClassPath(destination)) {
    // Destination resolved into one of the DEX files which are being compiled.
    // This is not a dependency.
    // Note that we do not have to test `source`:
    //   - case 1: `source` is not in class path
    //       Regardless of `is_assignable`, the test will always depend only on
    //       classes defined in the compiled dex files.
    //   - case 2: `source` is in the class path
    //       `Source` can never be a subtype of `destination`.
    return;
  }

  DexFileDeps* dex_deps = GetDexFileDeps(dex_file);
  if (dex_deps == nullptr) {
    // This invocation is from verification of a dex file which is not being compiled.
    return;
  }

  // Get string IDs for both descriptors and store in the appropriate set.

  std::string temp1, temp2;
  std::string destination_desc(destination->GetDescriptor(&temp1));
  std::string source_desc(source->GetDescriptor(&temp2));
  uint32_t destination_id = GetIdFromString(dex_file, destination_desc);
  uint32_t source_id = GetIdFromString(dex_file, source_desc);

  if (is_assignable) {
    dex_deps->assignable_types_.emplace(ClassPair(destination_id, source_id));
  } else {
    dex_deps->unassignable_types_.emplace(ClassPair(destination_id, source_id));
  }
}

void VerifierDeps::MaybeRecordClassResolution(MethodVerifier* verifier,
                                              uint16_t type_idx,
                                              mirror::Class* klass) {
  if (Runtime::Current()->GetVerifierDeps() != nullptr) {
    Runtime::Current()->GetVerifierDeps()->AddClassResolution(
        verifier->GetDexFile(), type_idx, klass);
  }
}

void VerifierDeps::MaybeRecordFieldResolution(MethodVerifier* verifier,
                                              uint32_t field_idx,
                                              ArtField* field) {
  if (Runtime::Current()->GetVerifierDeps() != nullptr) {
    Runtime::Current()->GetVerifierDeps()->AddFieldResolution(
        verifier->GetDexFile(), field_idx, field);
  }
}

void VerifierDeps::MaybeRecordMethodResolution(MethodVerifier* verifier,
                                               uint32_t method_idx,
                                               MethodResolutionType resolution_type,
                                               ArtMethod* method) {
  if (Runtime::Current()->GetVerifierDeps() != nullptr) {
    Runtime::Current()->GetVerifierDeps()->AddMethodResolution(
        verifier->GetDexFile(), method_idx, resolution_type, method);
  }
}

void VerifierDeps::MaybeRecordAssignability(MethodVerifier* verifier,
                                            mirror::Class* destination,
                                            mirror::Class* source,
                                            bool is_strict,
                                            bool is_assignable) {
  if (Runtime::Current()->GetVerifierDeps() != nullptr) {
    Runtime::Current()->GetVerifierDeps()->AddAssignability(
        verifier->GetDexFile(), destination, source, is_strict, is_assignable);
  }
}

}  // namespace verifier
}  // namespace art
