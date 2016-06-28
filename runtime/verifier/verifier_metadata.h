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

#ifndef ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_
#define ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_

#include <set>
#include <vector>

#include "art_field.h"
#include "art_method.h"
#include "base/mutex.h"
#include "os.h"

namespace art {

namespace mirror {
  class Class;
  class ClassLoader;
  class DexCache;
}  // namespace mirror

namespace verifier {

class VerifierMetadata;

enum MethodResolutionType {
  kDirectMethodResolution,
  kVirtualMethodResolution,
  kInterfaceMethodResolution,
};
std::ostream& operator<<(std::ostream& os, const MethodResolutionType& rhs);

class DexVerifierMetadata {
 public:
  explicit DexVerifierMetadata(const DexFile& dex_file, VerifierMetadata* parent);

  const DexFile& GetDexFile() const { return dex_file_; }
  bool IsSuccessfullyLoadedFromFile() const { return is_loaded_from_file_; }

  void RecordAssignabilityTest(mirror::Class* destination,
                               mirror::Class* source,
                               bool is_strict,
                               bool expected_outcome)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordClassResolution(uint16_t dex_type_idx, mirror::Class* klass)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordFieldResolution(uint32_t dex_field_idx, ArtField* field)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordMethodResolution(uint32_t dex_method_idx,
                              MethodResolutionType resolution_type,
                              ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_)
      REQUIRES(!lock_);

  void RecordSuccessfulVerification(const DexFile::ClassDef& class_def)
      REQUIRES(!lock_);

  bool IsClassVerified(const DexFile::ClassDef& class_def)
      REQUIRES(!lock_);

  void Clear()
      REQUIRES(!lock_);

  bool Verify(jobject class_loader, bool can_load_classes = true)
      REQUIRES(!lock_)
      NO_THREAD_SAFETY_ANALYSIS;  // Loading classes is not happy with holding `lock_`.
                                  // Method should not run multithreaded anyway.

  bool WriteToFile(File* file)
      REQUIRES(!lock_);

  void Dump(std::ostream& os)
      REQUIRES(!lock_);

  static constexpr uint16_t kUnresolvedMarker = static_cast<uint16_t>(-1);

 private:
  struct ClassPair : public std::tuple<uint32_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint32_t>;
    using Base::Base;
    ClassPair(const ClassPair&) = default;

    uint32_t GetDestination() const { return std::get<0>(*this); }
    uint32_t GetSource() const { return std::get<1>(*this); }
  };

  struct ClassResolutionTuple : public std::tuple<uint32_t, uint16_t> {
    using Base = std::tuple<uint32_t, uint16_t>;
    using Base::Base;
    ClassResolutionTuple(const ClassResolutionTuple&) = default;

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexTypeIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
  };

  struct FieldResolutionTuple : public std::tuple<uint32_t, uint16_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint16_t, uint32_t>;
    using Base::Base;
    FieldResolutionTuple(const FieldResolutionTuple&) = default;

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexFieldIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
    uint32_t GetDeclaringClass() const { return std::get<2>(*this); }
  };

  struct MethodResolutionTuple : public std::tuple<uint32_t, uint16_t, uint32_t> {
    using Base = std::tuple<uint32_t, uint16_t, uint32_t>;
    using Base::Base;
    MethodResolutionTuple(const MethodResolutionTuple&) = default;

    bool IsResolved() const { return GetAccessFlags() != kUnresolvedMarker; }
    uint32_t GetDexMethodIndex() const { return std::get<0>(*this); }
    uint16_t GetAccessFlags() const { return std::get<1>(*this); }
    uint32_t GetDeclaringClass() const { return std::get<2>(*this); }
  };

  bool IsInClassPath(mirror::Class* klass)
      SHARED_REQUIRES(Locks::mutator_lock_);

  uint32_t GetIdFromString(const std::string& str)
      REQUIRES(!lock_);

  std::string GetStringFromId(uint32_t id)
      REQUIRES(lock_);

  bool IsStringIdInDexFile(uint32_t id) const;

  bool ReadFromFile(File* file)
      NO_THREAD_SAFETY_ANALYSIS;  // The static method is not happy with this.

  void CompressAssignables(bool kind, std::set<std::tuple<std::string, std::string>>* seen)
      REQUIRES(!lock_);

  void CompressClasses(std::set<std::string>* seen)
      REQUIRES(!lock_);

  void CompressFields(std::set<std::tuple<std::string, std::string, std::string>>* seen)
      REQUIRES(!lock_);

  void CompressMethods(MethodResolutionType kind,
                       std::set<std::tuple<std::string, std::string, std::string>>* seen)
      REQUIRES(!lock_);

  const DexFile& dex_file_;
  const VerifierMetadata* parent_;
  Mutex lock_;

  bool is_loaded_from_file_;

  std::vector<std::string> strings_ GUARDED_BY(lock_);

  std::set<ClassPair> assignables_ GUARDED_BY(lock_);
  std::set<ClassPair> unassignables_ GUARDED_BY(lock_);

  std::set<ClassResolutionTuple> classes_ GUARDED_BY(lock_);
  std::set<FieldResolutionTuple> fields_ GUARDED_BY(lock_);
  std::set<MethodResolutionTuple> direct_methods_ GUARDED_BY(lock_);
  std::set<MethodResolutionTuple> virtual_methods_ GUARDED_BY(lock_);
  std::set<MethodResolutionTuple> interface_methods_ GUARDED_BY(lock_);

  std::vector<bool> verified_classes_ GUARDED_BY(lock_);

  static constexpr const char* kLockDescription = "VerifierMetadata lock";

  ART_FRIEND_TEST(DexVerifierMetadataTest, StringToId);
  friend class VerifierMetadata;
};

class VerifierMetadata {
 public:
  explicit VerifierMetadata(const std::vector<const DexFile*>& dex_files);

  DexVerifierMetadata* GetDexMetadataFor(const DexFile& dex_file) {
    for (auto&& entry : dex_metadata_) {
      if (&entry->GetDexFile() == &dex_file) {
        return entry.get();
      }
    }
    return nullptr;
  }

  bool IsSuccessfullyLoadedFromFile() const {
    return std::all_of(dex_metadata_.cbegin(), dex_metadata_.cend(),
        [](auto&& entry) { return entry->IsSuccessfullyLoadedFromFile(); });
  }

  bool Verify(jobject class_loader, bool can_load_classes = true) {
    for (auto&& entry : dex_metadata_) {
      if (!entry->Verify(class_loader, can_load_classes)) {
        return false;
      }
    }
    return true;
  }

  bool WriteToFile(File* file) {
    for (auto&& entry : dex_metadata_) {
      if (!entry->WriteToFile(file)) {
        return false;
      }
    }
    return true;
  }

  void Clear() {
    for (auto&& entry : dex_metadata_) {
      entry->Clear();
    }
  }

  void Compress()
      NO_THREAD_SAFETY_ANALYSIS;  // Accessing internals of DexVerifierMetadata instances.

  static VerifierMetadata* ReadFromFile(File* file, const std::vector<const DexFile*>& dex_files);

 private:
  std::vector<std::unique_ptr<DexVerifierMetadata>> dex_metadata_;

  friend class DexVerifierMetadata;
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_VERIFIER_METADATA_H_
