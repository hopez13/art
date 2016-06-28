
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

uint32_t VerifierMetadata::GetIdFromString(const std::string& str) {
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
  DCHECK_EQ(str, GetStringFromId_Internal(new_id));

  return new_id;
}

std::string VerifierMetadata::GetStringFromId_Internal(uint32_t string_id) {
  uint32_t num_ids_in_dex = dex_file_.NumStringIds();
  if (string_id < num_ids_in_dex) {
    return std::string(dex_file_.StringDataByIdx(string_id));
  } else {
    string_id -= num_ids_in_dex;
    CHECK_LT(string_id, strings_.size());
    return strings_[string_id];
  }
}

std::string VerifierMetadata::GetStringFromId(uint32_t string_id) {
  MutexLock mu(Thread::Current(), lock_);
  return GetStringFromId_Internal(string_id);
}

bool VerifierMetadata::IsStringIdInDexFile(uint32_t id) const {
  return id < dex_file_.NumStringIds();
}

inline static bool IsInBootClassPath(mirror::Class* klass)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  return klass->IsBootStrapClassLoaded();
  // DCHECK_EQ(result, ContainsElement(Runtime::Current()->GetClassLinker()->GetBootClassPath(),
  //                                   &klass->GetDexFile()));
}

void VerifierMetadata::RecordAssignabilityTest(mirror::Class* destination,
                                               mirror::Class* source,
                                               bool is_strict,
                                               bool is_assignable) {
  DCHECK(destination != nullptr && destination->IsResolved() && !destination->IsPrimitive());
  DCHECK(source != nullptr && source->IsResolved() && !source->IsPrimitive());

  if (!IsInBootClassPath(destination)) {
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
inline static uint32_t GetAccessFlags(T* element) SHARED_REQUIRES(Locks::mutator_lock_) {
  DCHECK(element != nullptr);

  uint32_t access_flags = element->GetAccessFlags() & kAccJavaFlagsMask;
  DCHECK_NE(access_flags, VerifierMetadata::kUnresolvedMarker);
  return access_flags;
}

void VerifierMetadata::RecordClassResolution(uint16_t dex_type_idx, mirror::Class* klass) {
  if (klass != nullptr) {
    if (!IsInBootClassPath(klass)) {
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

void VerifierMetadata::RecordFieldResolution(uint32_t dex_field_idx, ArtField* field) {
  if (field != nullptr && !IsInBootClassPath(field->GetDeclaringClass())) {
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

void VerifierMetadata::RecordMethodResolution(uint32_t dex_method_idx,
                                              MethodResolutionType resolution_type,
                                              ArtMethod* method) {
  if (method != nullptr && !IsInBootClassPath(method->GetDeclaringClass())) {
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

bool VerifierMetadata::Verify(jobject jclass_loader, bool can_load_classes) {
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
      const std::string& destination_desc = GetStringFromId_Internal(entry.GetDestination());
      mirror::Class* destination = RegTypeCache::ResolveClass(
          destination_desc.c_str(), class_loader.Get(), can_load_classes);
      const std::string& source_desc = GetStringFromId_Internal(entry.GetSource());
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
      std::string expected_decl_klass = GetStringFromId_Internal(entry.GetDeclaringClass());
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
      Signature signature = dex_file_.GetMethodSignature(method_id);

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
        std::string expected_decl_klass = GetStringFromId_Internal(entry.GetDeclaringClass());
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

void VerifierMetadata::Dump(std::ostream& os) {
  MutexLock mu(Thread::Current(), lock_);
  std::string temp;

  for (auto&& assignability : { std::make_pair(true, assignables_),
                                std::make_pair(false, unassignables_) }) {
    for (auto& entry : assignability.second) {
      os << "type " << GetStringFromId_Internal(entry.GetDestination())
         << (assignability.first ? "" : " not") << " assignable from "
         << GetStringFromId_Internal(entry.GetSource()) << std::endl;
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
         << GetStringFromId_Internal(entry.GetDeclaringClass()) << std::endl;
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
           << GetStringFromId_Internal(entry.GetDeclaringClass()) << std::endl;
      } else {
        os << "unresolved" << std::endl;
      }
    }
  }
}

inline static void OverwriteInt(uint32_t value, std::vector<uint8_t>::iterator pos) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
  std::vector<uint8_t> temp(data, data + sizeof(uint32_t));
  std::copy(temp.begin(), temp.end(), pos);
}

inline static void WriteInt(uint32_t value, std::vector<uint8_t>* out) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(&value);
  out->insert(out->end(), data, data + sizeof(uint32_t));
}

inline static void WriteString(const std::string& str, std::vector<uint8_t>* out) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(str.c_str());
  size_t length = str.length() + 1;
  DCHECK_EQ(0u, data[length - 1]);
  out->insert(out->end(), data, data + length);
}

bool VerifierMetadata::WriteToFile(File* file) {
  MutexLock mu(Thread::Current(), lock_);

  std::vector<uint8_t> buffer;

  // HEADER
  //  uint32_t    length
  //  uint32_t    dex_file_checksum

  WriteInt(0u, &buffer);  // placeholder
  WriteInt(dex_file_.GetHeader().checksum_, &buffer);

  // STRINGS

  WriteInt(strings_.size(), &buffer);
  for (size_t i = 0; i < strings_.size(); ++i) {
    WriteString(strings_[i], &buffer);
  }

  // EVERYTHING ELSE

  WriteInt(assignables_.size(), &buffer);
  for (auto& entry : assignables_) {
    WriteInt(entry.GetDestination(), &buffer);
    WriteInt(entry.GetSource(), &buffer);
  }

  WriteInt(unassignables_.size(), &buffer);
  for (auto& entry : unassignables_) {
    WriteInt(entry.GetDestination(), &buffer);
    WriteInt(entry.GetSource(), &buffer);
  }

  WriteInt(classes_.size(), &buffer);
  for (auto& entry : classes_) {
    WriteInt(entry.GetDexTypeIndex(), &buffer);
    WriteInt(entry.GetAccessFlags(), &buffer);
  }

  WriteInt(fields_.size(), &buffer);
  for (auto& entry : fields_) {
    WriteInt(entry.GetDexFieldIndex(), &buffer);
    WriteInt(entry.GetAccessFlags(), &buffer);
    WriteInt(entry.GetDeclaringClass(), &buffer);
  }

  WriteInt(direct_methods_.size(), &buffer);
  for (auto& entry : direct_methods_) {
    WriteInt(entry.GetDexMethodIndex(), &buffer);
    WriteInt(entry.GetAccessFlags(), &buffer);
    WriteInt(entry.GetDeclaringClass(), &buffer);
  }

  WriteInt(virtual_methods_.size(), &buffer);
  for (auto& entry : virtual_methods_) {
    WriteInt(entry.GetDexMethodIndex(), &buffer);
    WriteInt(entry.GetAccessFlags(), &buffer);
    WriteInt(entry.GetDeclaringClass(), &buffer);
  }

  WriteInt(interface_methods_.size(), &buffer);
  for (auto& entry : interface_methods_) {
    WriteInt(entry.GetDexMethodIndex(), &buffer);
    WriteInt(entry.GetAccessFlags(), &buffer);
    WriteInt(entry.GetDeclaringClass(), &buffer);
  }

  // Override the first four bytes with the actual data length.
  size_t buffer_length = buffer.size();
  CHECK_LT(buffer_length, std::numeric_limits<uint32_t>::max());
  OverwriteInt(static_cast<uint32_t>(buffer_length), buffer.begin());

  // Write to file.
  return file->WriteFully(buffer.data(), buffer_length);
}

inline static uint32_t ReadInt(uint8_t** buffer, uint8_t* end) {
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

bool VerifierMetadata::ReadFromFile(File* file) {
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

  {
    MutexLock mu(Thread::Current(), lock_);

    uint32_t num_strings = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_strings; ++i) {
      std::string str = ReadString(&cursor, end);
      strings_.push_back(str);
      DCHECK_EQ(str, GetStringFromId_Internal(dex_file_.NumStringIds() + i));
    }

    uint32_t num_assignables = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_assignables; ++i) {
      assignables_.emplace(ClassPair(ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }

    uint32_t num_unassignables = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_unassignables; ++i) {
      unassignables_.emplace(ClassPair(ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }

    uint32_t num_classes = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_classes; ++i) {
      classes_.emplace(ClassResolutionTuple(
          ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }

    uint32_t num_fields = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_fields; ++i) {
      fields_.emplace(FieldResolutionTuple(
          ReadInt(&cursor, end), ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }

    uint32_t num_direct_methods = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_direct_methods; ++i) {
      direct_methods_.emplace(MethodResolutionTuple(
          ReadInt(&cursor, end), ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }

    uint32_t num_virtual_methods = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_virtual_methods; ++i) {
      virtual_methods_.emplace(MethodResolutionTuple(
          ReadInt(&cursor, end), ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }

    uint32_t num_interface_methods = ReadInt(&cursor, end);
    for (uint32_t i = 0; i < num_interface_methods; ++i) {
      interface_methods_.emplace(MethodResolutionTuple(
          ReadInt(&cursor, end), ReadInt(&cursor, end), ReadInt(&cursor, end)));
    }
  }

  return true;
}

}  // namespace verifier
}  // namespace art
