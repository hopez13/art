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

#include "mirror/class-inl.h"

namespace art {
namespace verifier {

std::ostream& operator<<(std::ostream& os, const MethodResolutionType& rhs) {
  switch (rhs) {
    case kDirectMethodResolution:
      os << "direct";
      break;
    case kVirtualMethodResolution:
      os << "virtual";
      break;
    case kInterfaceMethodResolution:
      os << "interface";
      break;
  }
  return os;
}

VerifierDeps::VerifierDeps(const std::vector<const DexFile*>& compiled_dex_files)
    : lock_(kLockDescription, kVerifierDepsLock) {
  for (const DexFile* dex_file : compiled_dex_files) {
    DCHECK(dex_file != nullptr);
    dex_deps_[dex_file];  // Construct DexFileDeps struct for `dex_file`.
  }
  DCHECK_EQ(dex_deps_.size(), compiled_dex_files.size());
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
    uint32_t string_id = GetIdFromString(dex_file,
                                         nullptr /* deps */,
                                         element->GetDeclaringClass()->GetDescriptor(&temp));
    return string_id;
  }
}

uint32_t VerifierDeps::GetIdFromString(const DexFile& dex_file,
                                       DexFileDeps* deps,
                                       const std::string& str) {
  const DexFile::StringId* string_id = dex_file.FindStringId(str.c_str());
  if (string_id != nullptr) {
    // String is in the DEX file. Return its ID.
    return dex_file.GetIndexForStringId(*string_id);
  }

  // String is not in the DEX file. Assign a new ID to it which is higher than
  // the number of strings in the DEX file.

  if (deps == nullptr) {
    deps = GetDexFileDeps(dex_file);
  }

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
  DCHECK_EQ(str, GetStringFromId(dex_file, deps, new_id));

  return new_id;
}

std::string VerifierDeps::GetStringFromId(const DexFile& dex_file,
                                          DexFileDeps* deps,
                                          uint32_t string_id) {
  uint32_t num_ids_in_dex = dex_file.NumStringIds();
  if (string_id < num_ids_in_dex) {
    return std::string(dex_file.StringDataByIdx(string_id));
  } else {
    if (deps == nullptr) {
      deps = GetDexFileDeps(dex_file);
    }
    string_id -= num_ids_in_dex;
    CHECK_LT(string_id, deps->strings_.size());
    return deps->strings_[string_id];
  }
}

VerifierDeps::DexFileDeps* VerifierDeps::GetDexFileDeps(const DexFile& dex_file,
                                                        bool crash_if_not_found) {
  auto it = dex_deps_.find(&dex_file);
  if (it == dex_deps_.end()) {
    if (crash_if_not_found) {
      LOG(FATAL) << "Could not find DexFileDeps for DEX file " << dex_file.GetLocation();
      UNREACHABLE();
    } else {
      return nullptr;
    }
  }

  return &(it->second);
}

VerifierDeps::DexFileDeps* VerifierDeps::FindDexFileDepsForClass(mirror::Class* klass) {
  DCHECK(klass != nullptr);

  mirror::DexCache* dex_cache = klass->GetDexCache();
  if (dex_cache == nullptr) {
    return nullptr;
  }

  const DexFile* dex_file = dex_cache->GetDexFile();
  if (dex_file == nullptr) {
    return nullptr;
  }

  return GetDexFileDeps(*dex_file, false /* crash_if_not_found */);
}

bool VerifierDeps::IsInClassPath(mirror::Class* klass) {
  return (FindDexFileDepsForClass(klass) == nullptr);
}

void VerifierDeps::AddClassResolution(const DexFile& dex_file,
                                      uint16_t type_idx,
                                      mirror::Class* klass) {
  MutexLock mu(Thread::Current(), lock_);
  if (klass == nullptr || IsInClassPath(klass)) {
    GetDexFileDeps(dex_file)->classes_.emplace(ClassResolution(type_idx, GetAccessFlags(klass)));
  }
}

void VerifierDeps::AddFieldResolution(const DexFile& dex_file,
                                      uint32_t field_idx,
                                      ArtField* field) {
  MutexLock mu(Thread::Current(), lock_);
  if (field == nullptr || IsInClassPath(field->GetDeclaringClass())) {
    GetDexFileDeps(dex_file)->fields_.emplace(FieldResolution(
        field_idx,
        GetAccessFlags(field),
        GetDeclaringClassStringId(dex_file, field)));
  }
}

void VerifierDeps::AddMethodResolution(const DexFile& dex_file,
                                       uint32_t method_idx,
                                       MethodResolutionType resolution_type,
                                       ArtMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  if (method == nullptr || IsInClassPath(method->GetDeclaringClass())) {
    MethodResolution method_tuple(method_idx,
                                  GetAccessFlags(method),
                                  GetDeclaringClassStringId(dex_file, method));
    if (resolution_type == kDirectMethodResolution) {
      GetDexFileDeps(dex_file)->direct_methods_.emplace(method_tuple);
    } else if (resolution_type == kVirtualMethodResolution) {
      GetDexFileDeps(dex_file)->virtual_methods_.emplace(method_tuple);
    } else {
      DCHECK_EQ(resolution_type, kInterfaceMethodResolution);
      GetDexFileDeps(dex_file)->interface_methods_.emplace(method_tuple);
    }
  }
}

void VerifierDeps::AddAssignability(mirror::Class* destination,
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
    // Trivially not assignable.
    DCHECK(!is_assignable);
    return;
  }

  if (destination->IsArrayClass()) {
    // Both types are arrays. Solve recursively.
    DCHECK(source->IsArrayClass());
    AddAssignability(destination->GetComponentType(),
                     source->GetComponentType(),
                     /* is_strict */ true,
                     is_assignable);
    return;
  }

  MutexLock mu(Thread::Current(), lock_);

  if (!IsInClassPath(destination)) {
    // Assignability to a non-classpath class is not a dependency.
    return;
  }

  DCHECK_EQ(is_assignable, destination->IsAssignableFrom(source));

  // Find the optimal DEX file to store this assignability with. Our first choice
  // is a DEX file which contains a descriptor string for both `destination` and
  // `source`. If no such DEX file exists, we will settle for having at least one
  // of the two. Otherwise select the first DEX file.
  // Note that the deterministic properties of the algorithm automatically
  // deduplicate the assignability entries across all DexFileDeps.

  std::string temp1, temp2;
  std::string destination_desc(destination->GetDescriptor(&temp1));
  std::string source_desc(source->GetDescriptor(&temp2));

  std::pair<const DexFile *const, DexFileDeps>* optimal_dex = nullptr;
  std::pair<const DexFile *const, DexFileDeps>* suboptimal_dex = nullptr;
  for (auto& entry : dex_deps_) {
    const DexFile* dex_file = entry.first;
    bool has_destination_string = (dex_file->FindStringId(destination_desc.c_str()) != nullptr);
    bool has_source_string = (dex_file->FindStringId(source_desc.c_str()) != nullptr);

    if (has_destination_string && has_source_string) {
      optimal_dex = &entry;
      break;
    } else if (has_destination_string || has_source_string) {
      suboptimal_dex = &entry;
    }
  }

  std::pair<const DexFile *const, DexFileDeps>* target_dex = nullptr;
  if (optimal_dex != nullptr) {
    target_dex = optimal_dex;
  } else if (suboptimal_dex != nullptr) {
    target_dex = suboptimal_dex;
  } else {
    DCHECK(!dex_deps_.empty());
    target_dex = &(*dex_deps_.begin());
  }

  // Get string IDs for both descriptors and store in the appropriate set.

  uint32_t destination_string =
      GetIdFromString(*target_dex->first, &target_dex->second, destination_desc);
  uint32_t source_string = GetIdFromString(*target_dex->first, &target_dex->second, source_desc);

  if (is_assignable) {
    target_dex->second.assignable_types_.emplace(ClassPair(destination_string, source_string));
  } else {
    target_dex->second.unassignable_types_.emplace(ClassPair(destination_string, source_string));
  }
}

}  // namespace verifier
}  // namespace art
