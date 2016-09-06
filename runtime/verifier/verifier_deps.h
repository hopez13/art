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

#ifndef ART_RUNTIME_VERIFIER_VERIFIER_DEPS_H_
#define ART_RUNTIME_VERIFIER_VERIFIER_DEPS_H_

#include <map>
#include <set>
#include <vector>

#include "art_field.h"
#include "art_method.h"
#include "base/mutex.h"
#include "os.h"

namespace art {
namespace verifier {

enum MethodResolutionType {
  kDirectMethodResolution,
  kVirtualMethodResolution,
  kInterfaceMethodResolution,
};
std::ostream& operator<<(std::ostream& os, const MethodResolutionType& rhs);

class VerifierDeps {
 public:
  explicit VerifierDeps(const std::vector<const DexFile*>& compiled_dex_files);

  void AddClassResolution(const DexFile& dex_file, uint16_t type_idx, mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void AddFieldResolution(const DexFile& dex_file, uint32_t field_idx, ArtField* field)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void AddMethodResolution(const DexFile& dex_file,
                           uint32_t method_idx,
                           MethodResolutionType res_type,
                           ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void AddAssignability(mirror::Class* destination,
                        mirror::Class* source,
                        bool is_strict,
                        bool is_assignable)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(!lock_);

 private:
  struct ClassPair : public std::tuple<uint32_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint32_t>;
    using Base::Base;
    ClassPair(const ClassPair&) = default;

    uint32_t GetDestination() const { return std::get<0>(*this); }
    uint32_t GetSource() const { return std::get<1>(*this); }
  };

  struct ClassResolution : public std::tuple<uint32_t, uint16_t> {
    using Base = std::tuple<uint32_t, uint16_t>;
    using Base::Base;
    ClassResolution(const ClassResolution&) = default;

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexTypeIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
  };

  struct FieldResolution : public std::tuple<uint32_t, uint16_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint16_t, uint32_t>;
    using Base::Base;
    FieldResolution(const FieldResolution&) = default;

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexFieldIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
    uint32_t GetDeclaringClass() const { return std::get<2>(*this); }
  };

  struct MethodResolution : public std::tuple<uint32_t, uint16_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint16_t, uint32_t>;
    using Base::Base;
    MethodResolution(const MethodResolution&) = default;

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexMethodIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
    uint32_t GetDeclaringClass() const { return std::get<2>(*this); }
  };

  struct DexFileDeps {
    DexFileDeps() = default;

    // Vector of strings which are not present inside `dex_file_`. These are given
    // indices starting with `dex_file_->NumStringIds()`.
    std::vector<std::string> strings_;

    std::set<ClassPair> assignable_types_;
    std::set<ClassPair> unassignable_types_;

    std::set<ClassResolution> classes_;
    std::set<FieldResolution> fields_;
    std::set<MethodResolution> direct_methods_;
    std::set<MethodResolution> virtual_methods_;
    std::set<MethodResolution> interface_methods_;
   private:
    DISALLOW_COPY_AND_ASSIGN(DexFileDeps);
  };

  // Finds the DexFileDep instance associated with `dex_file`. If `crash_if_not_found`
  // is not set, it returns nullptr.
  DexFileDeps* GetDexFileDeps(const DexFile& dex_file, bool crash_if_not_found = true)
      REQUIRES(lock_);

  // Find the DexFileDep instance associated with the DEX file of `klass`, or
  // nullptr if it does not have a DEX file or if it is not defined in one of
  // the DEX files which are being compiled.
  DexFileDeps* FindDexFileDepsForClass(mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(lock_);

  // Returns true if `klass` is null or not defined in any of the compiled dex files.
  bool IsInClassPath(mirror::Class* klass)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(lock_);

  // Returns the index of `str`. If it is defined in `dex_file_`, this is the dex
  // string ID. If not, an ID is assigned to the string and cached in `strings_`
  // of the corresponding DexFileDeps structure (either provided or inferred from
  // `dex_file`).
  uint32_t GetIdFromString(const DexFile& dex_file, DexFileDeps* deps, const std::string& str)
      REQUIRES(lock_);

  // Returns the string represented by `id`.
  std::string GetStringFromId(const DexFile& dex_file, DexFileDeps* deps, uint32_t string_id)
      REQUIRES(lock_);

  // Returns the bytecode access flags of `element` (bottom 16 bits), or
  // `kUnresolvedMarker` if `element` is null.
  template <typename T>
  uint16_t GetAccessFlags(T* element)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns a string ID of the descriptor of the declaring class of `element`,
  // or `kUnresolvedMarker` if `element` is null.
  template <typename T>
  uint32_t GetDeclaringClassStringId(const DexFile& dex_file, T* element)
      REQUIRES_SHARED(Locks::mutator_lock_)
      REQUIRES(lock_);

  Mutex lock_;
  std::map<const DexFile*, DexFileDeps> dex_deps_ GUARDED_BY(lock_);

  static constexpr uint16_t kUnresolvedMarker = static_cast<uint16_t>(-1);
  static constexpr const char* kLockDescription = "VerifierMetadata lock";

  friend class VerifierDepsTest;
  ART_FRIEND_TEST(VerifierDepsTest, StringToId);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_DEPS_H_
