
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

#include "verifier_metadata.h"

#include <iostream>

#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "mirror/class-inl.h"
#include "reg_type_cache.h"
#include "runtime.h"
#include "utils.h"

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

DexVerifierMetadata::DexVerifierMetadata(const DexFile& dex_file, VerifierMetadata* parent)
    : dex_file_(dex_file),
      parent_(parent),
      lock_(kLockDescription, kVerifierMetadataLock) {
  Clear();
}

uint32_t DexVerifierMetadata::GetIdFromString(const std::string& str) {
  DCHECK(!IsSuccessfullyLoadedFromFile());

  const DexFile::StringId* string_id = dex_file_.FindStringId(str.c_str());
  if (string_id != nullptr) {
    return dex_file_.GetIndexForStringId(*string_id);
  }

  MutexLock mu(Thread::Current(), lock_);

  uint32_t num_ids_in_dex = dex_file_.NumStringIds();
  uint32_t num_extra_ids = strings_.size();

  for (size_t i = 0; i < num_extra_ids; ++i) {
    if (strings_[i] == str) {
      return num_ids_in_dex + i;
    }
  }

  strings_.push_back(str);

  uint32_t new_id = num_ids_in_dex + num_extra_ids;
  CHECK_GE(new_id, num_ids_in_dex);  // check for overflows
  DCHECK_EQ(str, GetStringFromId(new_id));

  return new_id;
}

std::string DexVerifierMetadata::GetStringFromId(uint32_t string_id) {
  uint32_t num_ids_in_dex = dex_file_.NumStringIds();
  if (string_id < num_ids_in_dex) {
    return std::string(dex_file_.StringDataByIdx(string_id));
  } else {
    string_id -= num_ids_in_dex;
    CHECK_LT(string_id, strings_.size());
    return strings_[string_id];
  }
}

bool DexVerifierMetadata::IsStringIdInDexFile(uint32_t id) const {
  return id < dex_file_.NumStringIds();
}

bool DexVerifierMetadata::IsInClassPath(mirror::Class* klass) {
  if (klass->GetDexCache() == nullptr) {
    return true;
  }
  if (klass->GetDexCache()->GetDexFile() == nullptr) {
    return true;
  }

  const DexFile& klass_dex_file = klass->GetDexFile();
  for (auto&& entry : parent_->dex_metadata_) {
    if (&entry->GetDexFile() == &klass_dex_file) {
      return false;
    }
  }
  return true;
}

void DexVerifierMetadata::RecordAssignabilityTest(mirror::Class* destination,
                                               mirror::Class* source,
                                               bool is_strict,
                                               bool is_assignable) {
  DCHECK(!IsSuccessfullyLoadedFromFile());
  DCHECK(destination != nullptr && destination->IsResolved() && !destination->IsPrimitive());
  DCHECK(source != nullptr && source->IsResolved() && !source->IsPrimitive());

  if (!IsInClassPath(destination)) {
    // Assignability to a non-boot classpath class is not a dependency.
    return;
  }

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
    RecordAssignabilityTest(destination->GetComponentType(),
                            source->GetComponentType(),
                            /* is_strict */ true,
                            is_assignable);
    return;
  }

  DCHECK_EQ(is_assignable, destination->IsAssignableFrom(source));

  std::string temp;
  uint32_t destination_string = GetIdFromString(destination->GetDescriptor(&temp));
  uint32_t source_string = GetIdFromString(source->GetDescriptor(&temp));

  {
    MutexLock mu(Thread::Current(), lock_);
    if (is_assignable) {
      assignables_.emplace(ClassPair(destination_string, source_string));
    } else {
      unassignables_.emplace(ClassPair(destination_string, source_string));
    }
  }
}

template <typename T>
inline static uint16_t GetAccessFlags(T* element) SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(element != nullptr);
  static_assert(kAccJavaFlagsMask == 0xFFFF, "Unexpected value of a constant");

  uint16_t access_flags = Low16Bits(element->GetAccessFlags());
  CHECK_NE(access_flags, DexVerifierMetadata::kUnresolvedMarker);
  return access_flags;
}

void DexVerifierMetadata::RecordClassResolution(uint16_t dex_type_idx, mirror::Class* klass) {
  DCHECK(!IsSuccessfullyLoadedFromFile());

  if (klass != nullptr) {
    if (!IsInClassPath(klass)) {
      return;
    } else if (klass->IsArrayClass()) {
      mirror::Class* component_type = klass->GetComponentType();
      while (component_type->IsArrayClass()) {
        component_type = component_type->GetComponentType();
      }
      if (component_type->IsPrimitive()) {
        return;
      }
    }
  }

  uint32_t access_flags = kUnresolvedMarker;
  if (klass != nullptr) {
    access_flags = GetAccessFlags(klass);
  }

  {
    MutexLock mu(Thread::Current(), lock_);
    classes_.emplace(ClassResolutionTuple(dex_type_idx, access_flags));
  }
}

void DexVerifierMetadata::RecordFieldResolution(uint32_t dex_field_idx, ArtField* field) {
  DCHECK(!IsSuccessfullyLoadedFromFile());

  if (field != nullptr && !IsInClassPath(field->GetDeclaringClass())) {
    // Field is declared in the loaded dex file. No boot classpath dependency to record.
    return;
  }

  uint32_t access_flags = kUnresolvedMarker;
  uint32_t declaring_klass = kUnresolvedMarker;
  if (field != nullptr) {
    std::string temp;
    access_flags = GetAccessFlags(field);
    declaring_klass = GetIdFromString(field->GetDeclaringClass()->GetDescriptor(&temp));
  }

  {
    MutexLock mu(Thread::Current(), lock_);
    fields_.emplace(FieldResolutionTuple(dex_field_idx, access_flags, declaring_klass));
  }
}

void DexVerifierMetadata::RecordMethodResolution(uint32_t dex_method_idx,
                                              MethodResolutionType resolution_type,
                                              ArtMethod* method) {
  DCHECK(!IsSuccessfullyLoadedFromFile());

  if (method != nullptr && !IsInClassPath(method->GetDeclaringClass())) {
    // Method is declared in the loaded dex file. No boot classpath dependency to record.
    return;
  }

  uint32_t access_flags = kUnresolvedMarker;
  uint32_t declaring_klass = kUnresolvedMarker;
  if (method != nullptr) {
    std::string temp;
    access_flags = GetAccessFlags(method);
    declaring_klass = GetIdFromString(method->GetDeclaringClass()->GetDescriptor(&temp));
  }

  {
    MutexLock mu(Thread::Current(), lock_);
    MethodResolutionTuple method_tuple(dex_method_idx, access_flags, declaring_klass);
    if (resolution_type == kDirectMethodResolution) {
      direct_methods_.emplace(method_tuple);
    } else if (resolution_type == kVirtualMethodResolution) {
      virtual_methods_.emplace(method_tuple);
    } else {
      DCHECK_EQ(resolution_type, kInterfaceMethodResolution);
      interface_methods_.emplace(method_tuple);
    }
  }
}

void DexVerifierMetadata::RecordSuccessfulVerification(const DexFile::ClassDef& class_def) {
  MutexLock mu(Thread::Current(), lock_);
  uint16_t class_def_idx = dex_file_.GetIndexForClassDef(class_def);
  verified_classes_[class_def_idx] = true;
}

bool DexVerifierMetadata::IsClassVerified(const DexFile::ClassDef& class_def) {
  MutexLock mu(Thread::Current(), lock_);
  uint16_t class_def_idx = dex_file_.GetIndexForClassDef(class_def);
  return verified_classes_[class_def_idx];
}

void DexVerifierMetadata::Clear() {
  MutexLock mu(Thread::Current(), lock_);
  strings_.clear();
  assignables_.clear();
  unassignables_.clear();
  classes_.clear();
  fields_.clear();
  direct_methods_.clear();
  virtual_methods_.clear();
  interface_methods_.clear();
  verified_classes_.resize(dex_file_.NumClassDefs(), /* default value */ false);
  is_loaded_from_file_ = false;
}

bool DexVerifierMetadata::Verify(jobject jclass_loader, bool can_load_classes) {
  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  auto pointer_size = class_linker->GetImagePointerSize();
  std::string temp;

  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader(
      hs.NewHandle(soa.Decode<mirror::ClassLoader*>(jclass_loader)));
  Handle<mirror::DexCache> dex_cache(hs.NewHandle(class_linker->FindDexCache(
      soa.Self(), dex_file_, false)));

  for (auto&& assignability : { std::make_pair(true, assignables_),
                                std::make_pair(false, unassignables_) }) {
    for (auto&& entry : assignability.second) {
      const std::string& destination_desc = GetStringFromId(entry.GetDestination());
      mirror::Class* destination = RegTypeCache::ResolveClass(
          destination_desc.c_str(), class_loader.Get(), can_load_classes);
      const std::string& source_desc = GetStringFromId(entry.GetSource());
      mirror::Class* source = RegTypeCache::ResolveClass(
          source_desc.c_str(), class_loader.Get(), can_load_classes);
      if (destination == nullptr) {
        LOG(ERROR) << "VeriFast: Could not resolve class " << destination_desc;
        return false;
      } else if (source == nullptr) {
        LOG(ERROR) << "VeriFast: Could not resolve class " << source_desc;
        return false;
      }
      DCHECK(destination->IsResolved() && source->IsResolved());
      if (destination->IsAssignableFrom(source) != assignability.first) {
        LOG(ERROR) << "VeriFast: Class " << destination_desc << " "
                   << (assignability.first ? "not" : "")<< " assignable from " << source_desc;
        return false;
      }
    }
  }

  for (auto&& entry : classes_) {
    const char* descriptor = dex_file_.StringByTypeIdx(entry.GetDexTypeIndex());
    mirror::Class* klass = RegTypeCache::ResolveClass(
        descriptor,
        class_loader.Get(),
        can_load_classes);
    DCHECK(klass == nullptr || klass->IsResolved());

    if (entry.IsResolved()) {
      if (klass == nullptr) {
        LOG(ERROR) << "VeriFast: Could not resolve class " << descriptor;
        return false;
      } else if (entry.GetAccessFlags() != GetAccessFlags(klass)) {
        LOG(ERROR) << "VeriFast: Unexpected access flags on class " << descriptor
                   << std::hex << " (expected=" << entry.GetAccessFlags()
                   << ", actual=" << GetAccessFlags(klass) << ")" << std::dec;
        return false;
      }
    } else if (klass != nullptr) {
      LOG(ERROR) << "VeriFast: Unexpected successful resolution of class " << descriptor;
      return false;
    }
  }

  for (auto&& entry : fields_) {
    ArtField* field = Runtime::Current()->GetClassLinker()->ResolveFieldJLS(
        dex_file_, entry.GetDexFieldIndex(), dex_cache, class_loader);
    if (field == nullptr) {
      DCHECK(Thread::Current()->IsExceptionPending());
      Thread::Current()->ClearException();
    }

    if (entry.IsResolved()) {
      std::string expected_decl_klass = GetStringFromId(entry.GetDeclaringClass());
      if (field == nullptr) {
        const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.GetDexFieldIndex());
        LOG(ERROR) << "VeriFast: Could not resolve field "
                   << dex_file_.GetFieldDeclaringClassDescriptor(field_id) << "->"
                   << dex_file_.GetFieldName(field_id) << ":"
                   << dex_file_.GetFieldTypeDescriptor(field_id);
        return false;
      } else if (expected_decl_klass != field->GetDeclaringClass()->GetDescriptor(&temp)) {
        const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.GetDexFieldIndex());
        LOG(ERROR) << "VeriFast: Unexpected declaring class for field resolution "
                   << dex_file_.GetFieldDeclaringClassDescriptor(field_id) << "->"
                   << dex_file_.GetFieldName(field_id) << ":"
                   << dex_file_.GetFieldTypeDescriptor(field_id)
                   << " (expected=" << expected_decl_klass
                   << ", actual=" << field->GetDeclaringClass()->GetDescriptor(&temp) << ")";
        return false;
      } else if (entry.GetAccessFlags() != GetAccessFlags(field)) {
        const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.GetDexFieldIndex());
        LOG(ERROR) << "VeriFast: Unexpected access flags for resolved field "
                   << dex_file_.GetFieldDeclaringClassDescriptor(field_id) << "->"
                   << dex_file_.GetFieldName(field_id) << ":"
                   << dex_file_.GetFieldTypeDescriptor(field_id)
                   << std::hex << " (expected=" << entry.GetAccessFlags()
                   << ", actual=" << GetAccessFlags(field) << ")" << std::dec;
        return false;
      }
    } else if (field != nullptr) {
      const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.GetDexFieldIndex());
      LOG(ERROR) << "VeriFast: Unexpected successful resolution of field "
                 << dex_file_.GetFieldDeclaringClassDescriptor(field_id) << "->"
                 << dex_file_.GetFieldName(field_id) << ":"
                 << dex_file_.GetFieldTypeDescriptor(field_id);
      return false;
    }
  }

  for (auto&& resolution : { std::make_pair(kDirectMethodResolution, direct_methods_),
                             std::make_pair(kVirtualMethodResolution, virtual_methods_),
                             std::make_pair(kInterfaceMethodResolution, interface_methods_) }) {
    for (auto&& entry : resolution.second) {
      const DexFile::MethodId& method_id = dex_file_.GetMethodId(entry.GetDexMethodIndex());
      const char* descriptor = dex_file_.GetMethodDeclaringClassDescriptor(method_id);
      mirror::Class* klass = RegTypeCache::ResolveClass(
          descriptor, class_loader.Get(), can_load_classes);
      if (klass == nullptr) {
        LOG(ERROR) << "VeriFast: Could not resolve class " << descriptor;
        return false;
      }
      DCHECK(klass->IsResolved());

      const char* name = dex_file_.GetMethodName(method_id);
      const Signature signature = dex_file_.GetMethodSignature(method_id);

      ArtMethod* method = nullptr;
      if (resolution.first == kDirectMethodResolution) {
        method = klass->FindDirectMethod(name, signature, pointer_size);
      } else if (resolution.first == kVirtualMethodResolution) {
        method = klass->FindVirtualMethod(name, signature, pointer_size);
      } else {
        DCHECK_EQ(resolution.first, kInterfaceMethodResolution);
        method = klass->FindInterfaceMethod(name, signature, pointer_size);
      }

      if (entry.IsResolved()) {
        std::string expected_decl_klass = GetStringFromId(entry.GetDeclaringClass());
        if (method == nullptr) {
          LOG(ERROR) << "VeriFast: Could not resolve " << resolution.first << " method "
                     << descriptor << "->" << name << signature.ToString();
          return false;
        } else if (expected_decl_klass != method->GetDeclaringClass()->GetDescriptor(&temp)) {
          LOG(ERROR) << "VeriFast: Unexpected declaring class for " << resolution.first
                     << " method resolution " << descriptor << "->" << name << signature.ToString()
                     << " (expected=" << expected_decl_klass
                     << ", actual=" << method->GetDeclaringClass()->GetDescriptor(&temp) << ")";
          return false;
        } else if (entry.GetAccessFlags() != GetAccessFlags(method)) {
          LOG(ERROR) << "VeriFast: Unexpected access flags for resolved " << resolution.first
                     << " method resolution " << descriptor << "->" << name << signature.ToString()
                     << std::hex << " (expected=" << entry.GetAccessFlags()
                     << ", actual=" << GetAccessFlags(method) << ")" << std::dec;
          return false;
        }
      } else if (method != nullptr) {
        LOG(ERROR) << "VeriFast: Unexpected successful resolution of " << resolution.first
                   << " method " << descriptor << "->" << name << signature.ToString();
        return false;
      }
    }
  }

  return true;
}

void DexVerifierMetadata::Dump(std::ostream& os) {
  MutexLock mu(Thread::Current(), lock_);
  std::string temp;

  for (auto&& assignability : { std::make_pair(true, assignables_),
                                std::make_pair(false, unassignables_) }) {
    for (auto& entry : assignability.second) {
      os << "type " << GetStringFromId(entry.GetDestination())
         << (assignability.first ? "" : " not") << " assignable from "
         << GetStringFromId(entry.GetSource()) << std::endl;
    }
  }

  for (auto& entry : classes_) {
    os << "class " << dex_file_.StringByTypeIdx(entry.GetDexTypeIndex()) << " "
       << (entry.IsResolved() ? PrettyJavaAccessFlags(entry.GetAccessFlags()) : "unresolved")
       << std::endl;
  }

  for (auto&& entry : fields_) {
    const DexFile::FieldId& field_id = dex_file_.GetFieldId(entry.GetDexFieldIndex());
    os << "field "
       << dex_file_.GetFieldDeclaringClassDescriptor(field_id) << "->"
       << dex_file_.GetFieldName(field_id) << ":"
       << dex_file_.GetFieldTypeDescriptor(field_id) << " ";
    if (entry.IsResolved()) {
      os << PrettyJavaAccessFlags(entry.GetAccessFlags())
         << "in "
         << GetStringFromId(entry.GetDeclaringClass()) << std::endl;
    } else {
      os << "unresolved" << std::endl;
    }
  }

  for (auto&& resolution : { std::make_pair(kDirectMethodResolution, direct_methods_),
                             std::make_pair(kVirtualMethodResolution, virtual_methods_),
                             std::make_pair(kInterfaceMethodResolution, interface_methods_) }) {
    for (auto&& entry : resolution.second) {
      const DexFile::MethodId& method_id = dex_file_.GetMethodId(entry.GetDexMethodIndex());
      os << resolution.first << " method "
         << dex_file_.GetMethodDeclaringClassDescriptor(method_id) << "->"
         << dex_file_.GetMethodName(method_id)
         << dex_file_.GetMethodSignature(method_id).ToString() << " ";
      if (entry.IsResolved()) {
        os << PrettyJavaAccessFlags(entry.GetAccessFlags())
           << "in "
           << GetStringFromId(entry.GetDeclaringClass()) << std::endl;
      } else {
        os << "unresolved" << std::endl;
      }
    }
  }

  for (size_t i = 0; i < dex_file_.NumClassDefs(); ++i) {
    const char* descriptor = dex_file_.GetClassDescriptor(dex_file_.GetClassDef(i));
    bool verified = verified_classes_[i];
    os << "verification of " << descriptor << (verified ? " successful" : " unsuccessful")
       << std::endl;
  }
}

inline static void OverwriteInt(uint32_t value, std::vector<uint8_t>::iterator pos) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
  std::vector<uint8_t> temp(data, data + sizeof(uint32_t));
  std::copy(temp.begin(), temp.end(), pos);
}

inline static void WriteInt16(uint16_t value, std::vector<uint8_t>* out) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
  out->insert(out->end(), data, data + sizeof(uint16_t));
}

inline static void WriteInt32(uint32_t value, std::vector<uint8_t>* out) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
  out->insert(out->end(), data, data + sizeof(uint32_t));
}

inline static void WriteString(const std::string& str, std::vector<uint8_t>* out) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
  size_t length = str.length() + 1;
  DCHECK_EQ(0u, data[length - 1]);
  out->insert(out->end(), data, data + length);
}

inline static size_t BytesToStoreBitVector(size_t bits) {
  if ((bits & (kBitsPerByte - 1)) == 0) {
    return bits >> kBitsPerByteLog2;
  } else {
    return (bits >> kBitsPerByteLog2) + 1;
  }
}

inline static void WriteBitVector(const std::vector<bool>& bitvector, std::vector<uint8_t>* out) {
  for (size_t i = 0; i < BytesToStoreBitVector(bitvector.size()); ++i) {
    uint8_t byte = 0u;
    for (size_t j = 0; j < kBitsPerByte; ++j) {
      size_t idx = (i << kBitsPerByteLog2) + j;
      if (idx < bitvector.size() && bitvector[i]) {
        byte |= (1u << j);
      }
    }
    out->push_back(byte);
  }
}

bool DexVerifierMetadata::WriteToFile(File* file) {
  MutexLock mu(Thread::Current(), lock_);

  std::vector<uint8_t> buffer;

  // HEADER
  //  uint32_t    length
  //  uint32_t    dex_file_checksum

  WriteInt32(0u, &buffer);  // placeholder
  WriteInt32(dex_file_.GetHeader().checksum_, &buffer);

  // STRINGS

  WriteInt32(strings_.size(), &buffer);
  for (size_t i = 0; i < strings_.size(); ++i) {
    WriteString(strings_[i], &buffer);
  }

  // EVERYTHING ELSE

  for (auto&& storage : { assignables_, unassignables_ }) {
    WriteInt32(storage.size(), &buffer);
    for (auto& entry : storage) {
      WriteInt32(entry.GetDestination(), &buffer);
      WriteInt32(entry.GetSource(), &buffer);
    }
  }

  WriteInt32(classes_.size(), &buffer);
  for (auto& entry : classes_) {
    WriteInt32(entry.GetDexTypeIndex(), &buffer);
    WriteInt16(entry.GetAccessFlags(), &buffer);
  }

  WriteInt32(fields_.size(), &buffer);
  for (auto& entry : fields_) {
    WriteInt32(entry.GetDexFieldIndex(), &buffer);
    WriteInt16(entry.GetAccessFlags(), &buffer);
    WriteInt32(entry.GetDeclaringClass(), &buffer);
  }

  for (auto&& storage : { direct_methods_, virtual_methods_, interface_methods_ }) {
    WriteInt32(storage.size(), &buffer);
    for (auto& entry : storage) {
      WriteInt32(entry.GetDexMethodIndex(), &buffer);
      WriteInt16(entry.GetAccessFlags(), &buffer);
      WriteInt32(entry.GetDeclaringClass(), &buffer);
    }
  }

  // TODO: Store list of not verified.

  WriteBitVector(verified_classes_, &buffer);

  // Override the first four bytes with the actual data length.
  size_t buffer_length = buffer.size();
  CHECK_LT(buffer_length, std::numeric_limits<uint32_t>::max());
  OverwriteInt(static_cast<uint32_t>(buffer_length), buffer.begin());

  // Write to file.
  return file->WriteFully(buffer.data(), buffer_length);
}

inline static uint16_t ReadInt16(uint8_t** buffer, uint8_t* end) {
  uint16_t value = *(reinterpret_cast<uint16_t*>(*buffer));
  *buffer += sizeof(uint16_t);
  CHECK_LE(*buffer, end);
  return value;
}

inline static uint32_t ReadInt32(uint8_t** buffer, uint8_t* end) {
  uint32_t value = *(reinterpret_cast<uint32_t*>(*buffer));
  *buffer += sizeof(uint32_t);
  CHECK_LE(*buffer, end);
  return value;
}

inline static std::string ReadString(uint8_t** buffer, uint8_t* end) {
  uint8_t* start = *buffer;
  while (**buffer != 0u) {
    (*buffer)++;
  }
  CHECK_LT(*buffer, end);
  return std::string(start, (*buffer)++);
}

inline static std::vector<bool> ReadBitVector(size_t num_bits, uint8_t** buffer, uint8_t* end) {
  std::vector<bool> result(num_bits);
  for (size_t i = 0; i < BytesToStoreBitVector(num_bits); ++i, (*buffer)++) {
    uint8_t byte = **buffer;
    for (size_t j = 0; j < kBitsPerByte; ++j) {
      size_t idx = (i << kBitsPerByteLog2) + j;
      if (idx < num_bits) {
        bool bit = ((byte & (1u << j)) != 0u);
        result[idx] = bit;
      }
    }
  }
  CHECK_LE(*buffer, end);
  return result;
}

bool DexVerifierMetadata::ReadFromFile(File* file) {
  uint32_t buffer_length = 0u;
  if (!file->ReadFully(&buffer_length, sizeof(uint32_t))) {
    return false;
  }

  uint32_t dex_file_checksum = 0u;
  if (!file->ReadFully(&dex_file_checksum, sizeof(uint32_t)) ||
      dex_file_.GetHeader().checksum_ != dex_file_checksum) {
    return false;
  }

  buffer_length -= 2*sizeof(uint32_t);
  std::vector<uint8_t> buffer(buffer_length);
  if (!file->ReadFully(buffer.data(), buffer_length)) {
    return false;
  }

  uint8_t* cursor = buffer.data();
  uint8_t* end = buffer.data() + buffer_length;

  // TODO: Check bounds of the integers.

  {
    MutexLock mu(Thread::Current(), lock_);

    uint32_t num_strings = ReadInt32(&cursor, end);
    for (uint32_t i = 0; i < num_strings; ++i) {
      std::string str = ReadString(&cursor, end);
      strings_.push_back(str);
      DCHECK_EQ(str, GetStringFromId(dex_file_.NumStringIds() + i));
    }

    for (auto storage : { &assignables_, &unassignables_ }) {
      uint32_t num_entries = ReadInt32(&cursor, end);
      for (uint32_t i = 0; i < num_entries; ++i) {
        storage->emplace(ClassPair(ReadInt32(&cursor, end), ReadInt32(&cursor, end)));
      }
    }

    uint32_t num_classes = ReadInt32(&cursor, end);
    for (uint32_t i = 0; i < num_classes; ++i) {
      classes_.emplace(ClassResolutionTuple(
          ReadInt32(&cursor, end), ReadInt16(&cursor, end)));
    }

    uint32_t num_fields = ReadInt32(&cursor, end);
    for (uint32_t i = 0; i < num_fields; ++i) {
      fields_.emplace(FieldResolutionTuple(
          ReadInt32(&cursor, end), ReadInt16(&cursor, end), ReadInt32(&cursor, end)));
    }

    for (auto storage : { &direct_methods_, &virtual_methods_, &interface_methods_ }) {
      uint32_t num_entries = ReadInt32(&cursor, end);
      for (uint32_t i = 0; i < num_entries; ++i) {
        storage->emplace(MethodResolutionTuple(
              ReadInt32(&cursor, end), ReadInt16(&cursor, end), ReadInt32(&cursor, end)));
      }
    }

    verified_classes_ = ReadBitVector(dex_file_.NumClassDefs(), &cursor, end);
  }

  is_loaded_from_file_ = true;
  return true;
}

VerifierMetadata::VerifierMetadata(const std::vector<const DexFile*>& dex_files) {
  dex_metadata_.reserve(dex_files.size());
  for (const DexFile* dex_file : dex_files) {
    CHECK(dex_file != nullptr);
    dex_metadata_.push_back(
        std::unique_ptr<DexVerifierMetadata>(new DexVerifierMetadata(*dex_file, this)));
  }
}

VerifierMetadata* VerifierMetadata::ReadFromFile(
    File* file, const std::vector<const DexFile*>& dex_files) {
  std::unique_ptr<VerifierMetadata> metadata(new VerifierMetadata(dex_files));

  for (auto&& dex_metadata : metadata->dex_metadata_) {
    if (!dex_metadata->ReadFromFile(file)) {
      return nullptr;
    }
  }

  return metadata.release();
}

void DexVerifierMetadata::CompressAssignables(
    bool kind, std::set<std::tuple<std::string, std::string>>* seen) {
  MutexLock mu(Thread::Current(), lock_);
  auto& storage = (kind ? assignables_ : unassignables_);
  for (auto it = storage.begin(); it != storage.end();) {
    bool first_seen = seen->emplace(std::make_tuple(GetStringFromId(it->GetDestination()),
                                                    GetStringFromId(it->GetSource()))).second;
    if (first_seen) {
      it++;
    } else {
      it = storage.erase(it);
    }
  }
}

void DexVerifierMetadata::CompressClasses(std::set<std::string>* seen) {
  MutexLock mu(Thread::Current(), lock_);
  for (auto it = classes_.begin(); it != classes_.end();) {
    bool first_seen = seen->emplace(dex_file_.StringByTypeIdx(it->GetDexTypeIndex())).second;
    if (first_seen) {
      it++;
    } else {
      it = classes_.erase(it);
    }
  }
}

void DexVerifierMetadata::CompressFields(
    std::set<std::tuple<std::string, std::string, std::string>>* seen) {
  MutexLock mu(Thread::Current(), lock_);
  for (auto it = fields_.begin(); it != fields_.end();) {
    const DexFile::FieldId& field_id = dex_file_.GetFieldId(it->GetDexFieldIndex());
    bool first_seen = seen->emplace(std::make_tuple(
        dex_file_.GetFieldDeclaringClassDescriptor(field_id),
        dex_file_.GetFieldName(field_id),
        dex_file_.GetFieldTypeDescriptor(field_id))).second;
    if (first_seen) {
      it++;
    } else {
      it = fields_.erase(it);
    }
  }
}

void DexVerifierMetadata::CompressMethods(
    MethodResolutionType kind, std::set<std::tuple<std::string, std::string, std::string>>* seen) {
  auto& storage = (kind == kDirectMethodResolution ? direct_methods_ :
                   kind == kVirtualMethodResolution ? virtual_methods_ :
                   interface_methods_);
  for (auto it = storage.begin(); it != storage.end();) {
    auto& method_id = dex_file_.GetMethodId(it->GetDexMethodIndex());
    bool first_seen = seen->emplace(std::make_tuple(
        dex_file_.GetMethodDeclaringClassDescriptor(method_id),
        dex_file_.GetMethodName(method_id),
        dex_file_.GetMethodSignature(method_id).ToString())).second;
    if (first_seen) {
      it++;
    } else {
      it = storage.erase(it);
    }
  }
}

void VerifierMetadata::Compress() {
  for (bool kind : { true, false }) {
    std::set<std::tuple<std::string, std::string>> seen;
    for (auto&& current : dex_metadata_) {
      current->CompressAssignables(kind, &seen);
    }
  }

  {
    std::set<std::string> seen;
    for (auto&& current : dex_metadata_) {
      current->CompressClasses(&seen);
    }
  }

  {
    std::set<std::tuple<std::string, std::string, std::string>> seen;
    for (auto&& current : dex_metadata_) {
      current->CompressFields(&seen);
    }
  }

  for (MethodResolutionType kind : { kDirectMethodResolution,
                                     kVirtualMethodResolution,
                                     kInterfaceMethodResolution }) {
    std::set<std::tuple<std::string, std::string, std::string>> seen;
    for (auto&& current : dex_metadata_) {
      current->CompressMethods(kind, &seen);
    }
  }
}

}  // namespace verifier
}  // namespace art
