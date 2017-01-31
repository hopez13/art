/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "profile_compilation_info.h"

#include "errno.h"
#include <limits.h>
#include <vector>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "art_method-inl.h"
#include "base/mutex.h"
#include "base/scoped_flock.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/unix_file/fd_file.h"
#include "jit/profiling_info.h"
#include "os.h"
#include "safe_map.h"
#include <iostream>

namespace art {

const uint8_t ProfileCompilationInfo::kProfileMagic[] = { 'p', 'r', 'o', '\0' };
const uint8_t ProfileCompilationInfo::kProfileVersion[] = { '0', '0', '2', '\0' };

static constexpr uint16_t kMaxDexFileKeyLength = PATH_MAX;

// Debug flag to ignore checksums when testing if a method or a class is present in the profile.
// Used to facilitate testing profile guided compilation across a large number of apps
// using the same test profile.
static constexpr bool kDebugIgnoreChecksum = false;

// Transform the actual dex location into relative paths.
// Note: this is OK because we don't store profiles of different apps into the same file.
// Apps with split apks don't cause trouble because each split has a different name and will not
// collide with other entries.
std::string ProfileCompilationInfo::GetProfileDexFileKey(const std::string& dex_location) {
  DCHECK(!dex_location.empty());
  size_t last_sep_index = dex_location.find_last_of('/');
  if (last_sep_index == std::string::npos) {
    return dex_location;
  } else {
    DCHECK(last_sep_index < dex_location.size());
    return dex_location.substr(last_sep_index + 1);
  }
}

bool ProfileCompilationInfo::AddMethodsAndClasses(
    const std::vector<OnlineProfileMethodInfo>& methods,
    const std::set<DexCacheResolvedClasses>& resolved_classes) {
  for (const OnlineProfileMethodInfo& method : methods) {
    // Build the offline version of the compilation info
    std::vector<OfflineProfileInlineCache> inline_caches;
    for (const OnlineProfileInlineCache& cache : method.inline_caches) {
      std::vector<OfflineProfileClassReference> classes;
      for (const OnlineProfileClassReference& classRef : cache.classes) {
        classes.emplace_back(OfflineDexReference(classRef.class_dex_ref->GetLocation(),
                                                 classRef.class_dex_ref->GetLocationChecksum()),
                             classRef.type_index);
      }
      inline_caches.emplace_back(cache.dex_pc, classes);
    }
    OfflineProfileMethodInfo pmi(OfflineDexReference(method.method_dex_ref->GetLocation(),
                                                     method.method_dex_ref->GetLocationChecksum()),
                                 method.dex_method_index,
                                 inline_caches);

    if (!AddMethodIndex(pmi)) {
      return false;
    }
  }
  for (const DexCacheResolvedClasses& dex_cache : resolved_classes) {
    if (!AddResolvedClasses(dex_cache)) {
      return false;
    }
  }
  return true;
}

bool ProfileCompilationInfo::MergeAndSave(const std::string& filename,
                                          uint64_t* bytes_written,
                                          bool force) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  ScopedFlock flock;
  std::string error;
  if (!flock.Init(filename.c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC, /* block */ false, &error)) {
    LOG(WARNING) << "Couldn't lock the profile file " << filename << ": " << error;
    return false;
  }

  int fd = flock.GetFile()->Fd();

  // Load the file but keep a copy around to be able to infer if the content has changed.
  ProfileCompilationInfo fileInfo;
  ProfileLoadSatus status = fileInfo.LoadInternal(fd, &error);
  if (status == kProfileLoadSuccess) {
    // Merge the content of file into the current object.
    if (MergeWith(fileInfo)) {
      // If after the merge we have the same data as what is the file there's no point
      // in actually doing the write. The file will be exactly the same as before.
      if (Equals(fileInfo)) {
        if (bytes_written != nullptr) {
          *bytes_written = 0;
        }
        return true;
      }
    } else {
      LOG(WARNING) << "Could not merge previous profile data from file " << filename;
      if (!force) {
        return false;
      }
    }
  } else if (force &&
        ((status == kProfileLoadVersionMismatch) || (status == kProfileLoadBadData))) {
      // Log a warning but don't return false. We will clear the profile anyway.
      LOG(WARNING) << "Clearing bad or obsolete profile data from file "
          << filename << ": " << error;
  } else {
    LOG(WARNING) << "Could not load profile data from file " << filename << ": " << error;
    return false;
  }

  // We need to clear the data because we don't support appending to the profiles yet.
  if (!flock.GetFile()->ClearContent()) {
    PLOG(WARNING) << "Could not clear profile file: " << filename;
    return false;
  }

  // This doesn't need locking because we are trying to lock the file for exclusive
  // access and fail immediately if we can't.
  bool result = Save(fd);
  if (result) {
    VLOG(profiler) << "Successfully saved profile info to " << filename
        << " Size: " << GetFileSizeBytes(filename);
    if (bytes_written != nullptr) {
      *bytes_written = GetFileSizeBytes(filename);
    }
  } else {
    VLOG(profiler) << "Failed to save profile info to " << filename;
  }
  return result;
}

// Returns true if all the bytes were successfully written to the file descriptor.
static bool WriteBuffer(int fd, const uint8_t* buffer, size_t byte_count) {
  while (byte_count > 0) {
    int bytes_written = TEMP_FAILURE_RETRY(write(fd, buffer, byte_count));
    if (bytes_written == -1) {
      return false;
    }
    byte_count -= bytes_written;  // Reduce the number of remaining bytes.
    buffer += bytes_written;  // Move the buffer forward.
  }
  return true;
}

// Add the string bytes to the buffer.
static void AddStringToBuffer(std::vector<uint8_t>* buffer, const std::string& value) {
  buffer->insert(buffer->end(), value.begin(), value.end());
}

// Insert each byte, from low to high into the buffer.
template <typename T>
static void AddUintToBuffer(std::vector<uint8_t>* buffer, T value) {
  for (size_t i = 0; i < sizeof(T); i++) {
    buffer->push_back((value >> (i * kBitsPerByte)) & 0xff);
  }
}

void ProfileCompilationInfo::GroupClassesByDex(
    const std::set<ClassRef>& classes,
    /*out*/SafeMap<uint8_t, std::vector<dex::TypeIndex>>* dex_to_classes_map) {
  for (const auto& classes_it : classes) {
    auto dex_it = dex_to_classes_map->FindOrAdd(classes_it.dex_profile_index,
                                               std::vector<dex::TypeIndex>());
    dex_it->second.push_back(classes_it.type_index);
  }
}

uint32_t ProfileCompilationInfo::GetMethodsRegionSize(const DexFileData& dex_data) {
  // method index && inline cache size
  uint16_t size = sizeof(uint16_t) * 2 * dex_data.method_map.size();
  for (const auto& method_it : dex_data.method_map) {
    const InlineCache& inline_cache = method_it.second;
    size += sizeof(uint16_t) * inline_cache.size();  // dex_pc;
    for (auto inline_cache_it : inline_cache) {
      const std::set<ClassRef>& classes = inline_cache_it.second;
      SafeMap<uint8_t, std::vector<dex::TypeIndex>> dex_to_classes_map;
      GroupClassesByDex(classes, &dex_to_classes_map);
      for (const auto& dex_it : dex_to_classes_map) {
        size += sizeof(uint8_t);  // dex profile index;
        size += sizeof(uint8_t);  // number of classes;
        const std::vector<dex::TypeIndex>& dex_classes = dex_it.second;
        size += sizeof(uint16_t) * dex_classes.size();  // the actual classes
      }
    }
  }
  return size;
}

void ProfileCompilationInfo::AddInlineCacheToBuffer(std::vector<uint8_t>* buffer,
                                                    const InlineCache& inline_cache) {
  DCHECK_LT(inline_cache.size(), std::numeric_limits<uint16_t>::max());
  AddUintToBuffer(buffer, static_cast<uint16_t>(inline_cache.size()));  // TODO(calin): CHECK the size
  if (inline_cache.size() == 0) {
    return;
  }
  for (auto inline_cache_it : inline_cache) {
    uint16_t dex_pc = inline_cache_it.first;
    const std::set<ClassRef>& classes = inline_cache_it.second;
    AddUintToBuffer(buffer, dex_pc);
    DCHECK_LT(classes.size(), static_cast<size_t>(std::numeric_limits<uint8_t>::max()));
    DCHECK_NE(classes.size(), 0u) << "InlineCache contains a dex_pc with 0 classes";

    SafeMap<uint8_t, std::vector<dex::TypeIndex>> dex_to_classes_map;
    GroupClassesByDex(classes, &dex_to_classes_map);
    AddUintToBuffer(buffer, static_cast<uint8_t>(dex_to_classes_map.size()));
    for (const auto& dex_it : dex_to_classes_map) {
      uint8_t dex_profile_index = dex_it.first;
      const std::vector<dex::TypeIndex>& dex_classes = dex_it.second;
      AddUintToBuffer(buffer, dex_profile_index);
      AddUintToBuffer(buffer, static_cast<uint8_t>(dex_classes.size()));  // TODO(calin CHECK the size)
      for (size_t i = 0; i < dex_classes.size(); i++) {
        AddUintToBuffer(buffer, dex_classes[i].index_);
      }
    }
  }
}

static constexpr size_t kLineHeaderSize =
    2 * sizeof(uint16_t) +  // class_set.size + dex_location.size
    2 * sizeof(uint32_t);   // method_map.size + checksum

/**
 * Serialization format:
 *    magic,version,number_of_lines
 *    dex_location1,number_of_methods1,number_of_classes1,dex_location_checksum1, \
 *        method_id11,method_id12...,class_id1,class_id2...
 *    dex_location2,number_of_methods2,number_of_classes2,dex_location_checksum2, \
 *        method_id21,method_id22...,,class_id1,class_id2...
 *    .....
 **/
bool ProfileCompilationInfo::Save(int fd) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  DCHECK_GE(fd, 0);

  // Cache at most 50KB before writing.
  static constexpr size_t kMaxSizeToKeepBeforeWriting = 50 * KB;
  // Use a vector wrapper to avoid keeping track of offsets when we add elements.
  std::vector<uint8_t> buffer;
  WriteBuffer(fd, kProfileMagic, sizeof(kProfileMagic));
  WriteBuffer(fd, kProfileVersion, sizeof(kProfileVersion));
  AddUintToBuffer(&buffer, static_cast<uint8_t>(info_.size()));
  std::cout << "CALIN ADD header";
  for (const auto& it : info_) {
    if (buffer.size() > kMaxSizeToKeepBeforeWriting) {
      if (!WriteBuffer(fd, buffer.data(), buffer.size())) {
        return false;
      }
      buffer.clear();
    }
    const std::string& dex_location = it.first;
    const DexFileData& dex_data = it.second;
    // if (dex_data.method_map.empty() && dex_data.class_set.empty()) {
    //   continue;
    // }

    if (dex_location.size() >= kMaxDexFileKeyLength) {
      LOG(WARNING) << "DexFileKey exceeds allocated limit";
      return false;
    }

    // Make sure that the buffer has enough capacity to avoid repeated resizings
    // while we add data.
    uint32_t methods_region_size = GetMethodsRegionSize(dex_data);
    size_t required_capacity = buffer.size() +
        kLineHeaderSize +
        dex_location.size() +
        sizeof(uint16_t) * dex_data.class_set.size() +
        methods_region_size;
    std::cout << " write method region  " << methods_region_size << "\n";
    buffer.reserve(required_capacity);
    // TODO group 16 writes
    DCHECK_LE(dex_location.size(), std::numeric_limits<uint16_t>::max());
    DCHECK_LE(dex_data.class_set.size(), std::numeric_limits<uint16_t>::max());
    AddUintToBuffer(&buffer, static_cast<uint16_t>(dex_location.size()));
    AddUintToBuffer(&buffer, methods_region_size);
    AddUintToBuffer(&buffer, static_cast<uint16_t>(dex_data.class_set.size()));
    AddUintToBuffer(&buffer, dex_data.checksum);  // uint32_t

    AddStringToBuffer(&buffer, dex_location);

    for (const auto& method_it : dex_data.method_map) {
      AddUintToBuffer(&buffer, method_it.first);
      std::cout << " add method " << method_it.first << "\n";
      AddInlineCacheToBuffer(&buffer, method_it.second);
    }
    for (const auto& class_id : dex_data.class_set) {
      AddUintToBuffer(&buffer, class_id.index_);
    }
    DCHECK_LE(required_capacity, buffer.size())
        << "Failed to add the expected number of bytes in the buffer";
  }

  return WriteBuffer(fd, buffer.data(), buffer.size());
}

ProfileCompilationInfo::DexFileData* ProfileCompilationInfo::GetOrAddDexFileData(
    const std::string& dex_location,
    uint32_t checksum) {
  auto info_it = info_.FindOrAdd(dex_location, DexFileData(checksum, info_.size()));
  if (info_it->second.checksum != checksum) {
    LOG(WARNING) << "Checksum mismatch for dex " << dex_location;
    return nullptr;
  }
  return &info_it->second;
}

bool ProfileCompilationInfo::AddResolvedClasses(const DexCacheResolvedClasses& classes) {
  const std::string dex_location = GetProfileDexFileKey(classes.GetDexLocation());
  const uint32_t checksum = classes.GetLocationChecksum();
  DexFileData* const data = GetOrAddDexFileData(dex_location, checksum);
  if (data == nullptr) {
    return false;
  }
  data->class_set.insert(classes.GetClasses().begin(), classes.GetClasses().end());
  return true;
}

bool ProfileCompilationInfo::AddMethodIndex(const std::string& dex_location,
                                            uint32_t checksum,
                                            uint16_t method_index) {
  DexFileData* const data = GetOrAddDexFileData(GetProfileDexFileKey(dex_location), checksum);
  if (data == nullptr) {
    return false;
  }

  data->method_map.Put(method_index, InlineCache());

  return true;
}

bool ProfileCompilationInfo::AddMethodIndex(const OfflineProfileMethodInfo& pmi) {
  DexFileData* const data = GetOrAddDexFileData(GetProfileDexFileKey(pmi.method_dex_ref.dex_location),
                                                pmi.method_dex_ref.dex_checksum);
  if (data == nullptr) {
    return false;
  }
  auto ic_it = data->method_map.FindOrAdd(pmi.dex_method_index, InlineCache());
  std::cout << "add method\n";
  for (const OfflineProfileInlineCache& inline_cache : pmi.inline_caches) {
    auto classes_it = ic_it->second.FindOrAdd(inline_cache.dex_pc, std::set<ClassRef>());

    for (const OfflineProfileClassReference& class_ref : inline_cache.classes) {
      const auto dex_ref = class_ref.class_dex_ref;
      DexFileData* class_dex_data = GetOrAddDexFileData(GetProfileDexFileKey(dex_ref.dex_location), dex_ref.dex_checksum);
      if (class_dex_data == nullptr) {
        return false;
      }
      classes_it->second.emplace(class_dex_data->profile_index, class_ref.type_index);
    }
  }
  return true;
}

bool ProfileCompilationInfo::AddClassIndex(const std::string& dex_location,
                                           uint32_t checksum,
                                           dex::TypeIndex type_idx) {
  DexFileData* const data = GetOrAddDexFileData(dex_location, checksum);
  if (data == nullptr) {
    return false;
  }
  data->class_set.insert(type_idx);
  return true;
}

bool ProfileCompilationInfo::ReadInlineCache(SafeBuffer& line_buffer,
                                             uint8_t dex_file_count,
                                             /*out*/ InlineCache* inline_cache,
                                             /*out*/ std::string* error) {
  uint16_t inline_cache_size;
  if (!line_buffer.ReadUintAndAdvance<uint16_t>(&inline_cache_size)) {
    *error = "Could not read inline_cache_size";
    return false;
  }
  for (; inline_cache_size > 0; inline_cache_size--) {
    uint16_t dex_pc;
    uint8_t dex_to_classes_map_size;
    auto classes_it = inline_cache->FindOrAdd(dex_pc, std::set<ClassRef>());
    if (!line_buffer.ReadUintAndAdvance<uint16_t>(&dex_pc)) {
      *error = "Could not read dex_pc";
      return false;
    }
    if (!line_buffer.ReadUintAndAdvance<uint8_t>(&dex_to_classes_map_size)) {
      *error = "Could not read dex_to_classes_map_size";
      return false;
    }
    for (; dex_to_classes_map_size > 0; dex_to_classes_map_size--) {
      uint8_t dex_profile_index;
      uint8_t dex_classes_size;
      if (!line_buffer.ReadUintAndAdvance<uint8_t>(&dex_profile_index)) {
        *error = "Could not read dex_profile_index";
        return false;
      }
      if (!line_buffer.ReadUintAndAdvance<uint8_t>(&dex_classes_size)) {
        *error = "Could not read dex_profile_index";
        return false;
      }
      if (dex_profile_index >= dex_file_count) {
         *error = "dex_profile_index out of bounds";
          return false;
      }
      for (; dex_classes_size > 0; dex_classes_size--) {
        uint16_t type_index;
        if (!line_buffer.ReadUintAndAdvance<uint16_t>(&type_index)) {
          *error = "Could not read type_index";
          return false;
        }
        classes_it->second.emplace(dex_profile_index, dex::TypeIndex(type_index));
      }
    }
  }
  return true;
}

bool ProfileCompilationInfo::ReadMethods(SafeBuffer& buffer,
                                         uint8_t dex_file_count,
                                         const ProfileLineHeader& line_header,
                                         /*out*/ std::string* error) {
  while (buffer.HasMoreData()) {
    DexFileData* const data = GetOrAddDexFileData(line_header.dex_location, line_header.checksum);
    uint16_t method_index;
    if (!buffer.ReadUintAndAdvance<uint16_t>(&method_index)) {
      *error = "Could not read method_index";
      return false;
    }
    std::cout << " read method " << method_index << "\n";
    auto it = data->method_map.FindOrAdd(method_index, InlineCache());
    if (!ReadInlineCache(buffer, dex_file_count, &(it->second), error)) {
      return false;
    }
  }

  return true;
}

bool ProfileCompilationInfo::ReadClasses(SafeBuffer& buffer,
                                         uint16_t classes_to_read,
                                         const ProfileLineHeader& line_header) {
  for (uint16_t i = 0; i < classes_to_read; i++) {
    uint16_t type_index;
    if (!buffer.ReadUintAndAdvance<uint16_t>(&type_index)) return false;
    if (!AddClassIndex(line_header.dex_location,
                       line_header.checksum,
                       dex::TypeIndex(type_index))) {
      return false;
    }
  }
  return true;
}

// Tests for EOF by trying to read 1 byte from the descriptor.
// Returns:
//   0 if the descriptor is at the EOF,
//  -1 if there was an IO error
//   1 if the descriptor has more content to read
static int testEOF(int fd) {
  uint8_t buffer[1];
  return TEMP_FAILURE_RETRY(read(fd, buffer, 1));
}

// Reads an uint value previously written with AddUintToBuffer.
template <typename T>
bool ProfileCompilationInfo::SafeBuffer::ReadUintAndAdvance(/*out*/ T* value) {
  static_assert(std::is_unsigned<T>::value, "Type is not unsigned");
  if (ptr_current_ + sizeof(T) > ptr_end_) {
    return false;
  }
  *value = 0;
  for (size_t i = 0; i < sizeof(T); i++) {
    *value += ptr_current_[i] << (i * kBitsPerByte);
  }
  ptr_current_ += sizeof(T);
  return true;
}

bool ProfileCompilationInfo::SafeBuffer::CompareAndAdvance(const uint8_t* data, size_t data_size) {
  if (ptr_current_ + data_size > ptr_end_) {
    return false;
  }
  if (memcmp(ptr_current_, data, data_size) == 0) {
    ptr_current_ += data_size;
    return true;
  }
  return false;
}

bool ProfileCompilationInfo::SafeBuffer::HasMoreData() {
  return ptr_current_ < ptr_end_;
}

ProfileCompilationInfo::ProfileLoadSatus ProfileCompilationInfo::SafeBuffer::FillFromFd(
      int fd,
      const std::string& source,
      /*out*/std::string* error) {
  size_t byte_count = ptr_end_ - ptr_current_;
  uint8_t* buffer = ptr_current_;
  while (byte_count > 0) {
    int bytes_read = TEMP_FAILURE_RETRY(read(fd, buffer, byte_count));
    if (bytes_read == 0) {
      *error += "Profile EOF reached prematurely for " + source;
      return kProfileLoadBadData;
    } else if (bytes_read < 0) {
      *error += "Profile IO error for " + source + strerror(errno);
      return kProfileLoadIOError;
    }
    byte_count -= bytes_read;
    buffer += bytes_read;
  }
  return kProfileLoadSuccess;
}

ProfileCompilationInfo::ProfileLoadSatus ProfileCompilationInfo::ReadProfileHeader(
      int fd,
      /*out*/uint8_t* number_of_lines,
      /*out*/std::string* error) {
  // Read magic and version
  const size_t kMagicVersionSize =
    sizeof(kProfileMagic) +
    sizeof(kProfileVersion) +
    sizeof(uint8_t);  // number of lines

  SafeBuffer safe_buffer(kMagicVersionSize);

  ProfileLoadSatus status = safe_buffer.FillFromFd(fd, "ReadProfileHeader", error);
  if (status != kProfileLoadSuccess) {
    return status;
  }

  if (!safe_buffer.CompareAndAdvance(kProfileMagic, sizeof(kProfileMagic))) {
    *error = "Profile missing magic";
    return kProfileLoadVersionMismatch;
  }
  if (!safe_buffer.CompareAndAdvance(kProfileVersion, sizeof(kProfileVersion))) {
    *error = "Profile version mismatch";
    return kProfileLoadVersionMismatch;
  }
  if (!safe_buffer.ReadUintAndAdvance<uint8_t>(number_of_lines)) return kProfileLoadBadData;
  return kProfileLoadSuccess;
}

ProfileCompilationInfo::ProfileLoadSatus ProfileCompilationInfo::ReadProfileLineHeader(
      int fd,
      /*out*/ProfileLineHeader* line_header,
      /*out*/std::string* error) {
  SafeBuffer header_buffer(kLineHeaderSize);
  ProfileLoadSatus status = header_buffer.FillFromFd(fd, "ReadProfileLineHeader", error);
  if (status != kProfileLoadSuccess) {
    return status;
  }

  uint16_t dex_location_size;
  if (!header_buffer.ReadUintAndAdvance<uint16_t>(&dex_location_size)) {
    return kProfileLoadBadData;
  }
  if (!header_buffer.ReadUintAndAdvance<uint32_t>(&line_header->method_map_size)) {
    return kProfileLoadBadData;
  }
  if (!header_buffer.ReadUintAndAdvance<uint16_t>(&line_header->class_set_size)) {
    return kProfileLoadBadData;
  }
  if (!header_buffer.ReadUintAndAdvance<uint32_t>(&line_header->checksum)) {
    return kProfileLoadBadData;
  }

  if (dex_location_size == 0 || dex_location_size > kMaxDexFileKeyLength) {
    *error = "DexFileKey has an invalid size: " +
        std::to_string(static_cast<uint32_t>(dex_location_size));
    return kProfileLoadBadData;
  }

  SafeBuffer location_buffer(dex_location_size);
  status = location_buffer.FillFromFd(fd, "ReadProfileHeaderDexLocation", error);
  if (status != kProfileLoadSuccess) {
    return status;
  }
  line_header->dex_location.assign(
      reinterpret_cast<char*>(location_buffer.Get()), dex_location_size);
  return kProfileLoadSuccess;
}

ProfileCompilationInfo::ProfileLoadSatus ProfileCompilationInfo::ReadProfileLine(
      int fd,
      uint8_t dex_file_count,
      const ProfileLineHeader& line_header,
      /*out*/std::string* error) {
  {
    SafeBuffer buffer(line_header.method_map_size);
    ProfileLoadSatus status = buffer.FillFromFd(fd, "ReadProfileLineMethods", error);
    if (status != kProfileLoadSuccess) {
      return status;
    }
    std::cout << " read method region  " << line_header.method_map_size << "\n";
    if (!ReadMethods(buffer, dex_file_count, line_header, error)) {
      return kProfileLoadBadData;
    }
  }

  {
    SafeBuffer buffer(sizeof(uint16_t) * line_header.class_set_size);
    ProfileLoadSatus status = buffer.FillFromFd(fd, "ReadProfileLineClasses", error);
    if (status != kProfileLoadSuccess) {
      return status;
    }
    if (!ReadClasses(buffer, line_header.class_set_size, line_header)) {
      *error = "Error when reading profile file line: classes";
      return kProfileLoadBadData;
    }
  }

  return kProfileLoadSuccess;
}

bool ProfileCompilationInfo::Load(int fd) {
  std::string error;
  ProfileLoadSatus status = LoadInternal(fd, &error);

  if (status == kProfileLoadSuccess) {
    return true;
  } else {
    PLOG(WARNING) << "Error when reading profile " << error;
    std::cout << "Error when reading profile " << error << "\n";
    return false;
  }
}

ProfileCompilationInfo::ProfileLoadSatus ProfileCompilationInfo::LoadInternal(
      int fd, std::string* error) {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  DCHECK_GE(fd, 0);

  struct stat stat_buffer;
  if (fstat(fd, &stat_buffer) != 0) {
    return kProfileLoadIOError;
  }
  // We allow empty profile files.
  // Profiles may be created by ActivityManager or installd before we manage to
  // process them in the runtime or profman.
  if (stat_buffer.st_size == 0) {
    return kProfileLoadSuccess;
  }
  // Read profile header: magic + version + number_of_lines.
  uint8_t number_of_lines;
  ProfileLoadSatus status = ReadProfileHeader(fd, &number_of_lines, error);
  if (status != kProfileLoadSuccess) {
    return status;
  }

  for (int i = 0; i < number_of_lines; i++) {
    ProfileLineHeader line_header;
    std::cout << "CALIN: line " << i << "\n";
    // First, read the line header to get the amount of data we need to read.
    status = ReadProfileLineHeader(fd, &line_header, error);
    if (status != kProfileLoadSuccess) {
      return status;
    }

    // Now read the actual profile line.
    status = ReadProfileLine(fd, number_of_lines, line_header, error);
    if (status != kProfileLoadSuccess) {
      return status;
    }
  }

  // Check that we read everything and that profiles don't contain junk data.
  int result = testEOF(fd);
  if (result == 0) {
    return kProfileLoadSuccess;
  } else if (result < 0) {
    return kProfileLoadIOError;
  } else {
    *error = "Unexpected content in the profile file";
    return kProfileLoadBadData;
  }
}

bool ProfileCompilationInfo::MergeWith(const ProfileCompilationInfo& other) {
  // First verify that all checksums match. This will avoid adding garbage to
  // the current profile info.
  // Note that the number of elements should be very small, so this should not
  // be a performance issue.
  for (const auto& other_it : other.info_) {
    auto info_it = info_.find(other_it.first);
    if ((info_it != info_.end()) && (info_it->second.checksum != other_it.second.checksum)) {
      LOG(WARNING) << "Checksum mismatch for dex " << other_it.first;
      return false;
    }
  }
  // All checksums match. Import the data.
  SafeMap<uint8_t, uint8_t> dex_profile_index_remap;
  for (const auto& other_it : other.info_) {
    const std::string& other_dex_location = other_it.first;
    const DexFileData& other_dex_data = other_it.second;
    auto info_it = info_.FindOrAdd(other_dex_location, DexFileData(other_dex_data.checksum, 0));
    dex_profile_index_remap.Put(other_it.dex_profile_index, info_it.dex_profile_index);
  }

  for (const auto& other_it : other.info_) {
    const std::string& other_dex_location = other_it.first;
    const DexFileData& other_dex_data = other_it.second;
    auto info_it = info_.find(other_dex_location);
    DCHECK_NE(info_it, info_.end());

    info_it->second.class_set.insert(other_dex_data.class_set.begin(),
                                   other_dex_data.class_set.end());

    for (const auto& other_method_it : other_dex_data.method_map) {
      auto method_it = info_it->second.method_map.FindOrAdd(other_method_it->first, InlineCache());
      const auto& other_inline_cache = other_method_it.second;
      for (const auto& other_ic_it : other_inline_cache) {
        uint16_t other_dex_pc = other_ic_it->first;
        const std::set<ClassRef>& other_class_set = other_ic_it->second;
        auto class_set = method_it->second.FindOrAdd(other_dex_pc, std::set<ClassRef>());
        for (const auto& class_it : other_class_set) {
          class_set.emplace(dex_profile_index_remap.Get(
              class_it.dex_profile_index), class_it.type_index);
        }
      }
    }
  }
  return true;
}

static bool ChecksumMatch(const DexFile& dex_file, uint32_t checksum) {
  return kDebugIgnoreChecksum || dex_file.GetLocationChecksum() == checksum;
}

bool ProfileCompilationInfo::ContainsMethod(const MethodReference& method_ref) const {
  auto info_it = info_.find(GetProfileDexFileKey(method_ref.dex_file->GetLocation()));
  std::cout << "search " << GetProfileDexFileKey(method_ref.dex_file->GetLocation()) << "\n";
  if (info_it != info_.end()) {
    if (!ChecksumMatch(*method_ref.dex_file, info_it->second.checksum)) {
      std::cout << "checksum\n" << DumpInfo(nullptr, false)  << "\n";

      return false;
    }
    const MethodMap& methods = info_it->second.method_map;
    return methods.find(method_ref.dex_method_index) != methods.end();
  }
  std::cout << "not found\n" << DumpInfo(nullptr)  << "\n";
  return false;
}

bool ProfileCompilationInfo::ContainsClass(const DexFile& dex_file, dex::TypeIndex type_idx) const {
  auto info_it = info_.find(GetProfileDexFileKey(dex_file.GetLocation()));
  if (info_it != info_.end()) {
    if (!ChecksumMatch(dex_file, info_it->second.checksum)) {
      return false;
    }
    const std::set<dex::TypeIndex>& classes = info_it->second.class_set;
    return classes.find(type_idx) != classes.end();
  }
  return false;
}

uint32_t ProfileCompilationInfo::GetNumberOfMethods() const {
  uint32_t total = 0;
  for (const auto& it : info_) {
    total += it.second.method_map.size();
  }
  return total;
}

uint32_t ProfileCompilationInfo::GetNumberOfResolvedClasses() const {
  uint32_t total = 0;
  for (const auto& it : info_) {
    total += it.second.class_set.size();
  }
  return total;
}

std::string ProfileCompilationInfo::DumpInfo(const std::vector<const DexFile*>* dex_files,
                                             bool print_full_dex_location) const {
  std::ostringstream os;
  if (info_.empty()) {
    return "ProfileInfo: empty";
  }

  os << "ProfileInfo:";

  const std::string kFirstDexFileKeySubstitute = ":classes.dex";
  for (const auto& it : info_) {
    os << "\n";
    const std::string& location = it.first;
    const DexFileData& dex_data = it.second;
    if (print_full_dex_location) {
      os << location;
    } else {
      // Replace the (empty) multidex suffix of the first key with a substitute for easier reading.
      std::string multidex_suffix = DexFile::GetMultiDexSuffix(location);
      os << (multidex_suffix.empty() ? kFirstDexFileKeySubstitute : multidex_suffix);
    }
    const DexFile* dex_file = nullptr;
    if (dex_files != nullptr) {
      for (size_t i = 0; i < dex_files->size(); i++) {
        if (location == (*dex_files)[i]->GetLocation()) {
          dex_file = (*dex_files)[i];
        }
      }
    }
    os << "\n\tmethods: ";
    for (const auto method_it : dex_data.method_map) {
      if (dex_file != nullptr) {
        os << "\n\t\t" << dex_file->PrettyMethod(method_it.first, true);
      } else {
        os << method_it.first << ",";
      }
    }
    os << "\n\tclasses: ";
    for (const auto class_it : dex_data.class_set) {
      if (dex_file != nullptr) {
        os << "\n\t\t" << dex_file->PrettyType(class_it);
      } else {
        os << class_it << ",";
      }
    }
  }
  return os.str();
}

bool ProfileCompilationInfo::Equals(const ProfileCompilationInfo& other) {
  return info_.Equals(other.info_);
}

std::set<DexCacheResolvedClasses> ProfileCompilationInfo::GetResolvedClasses() const {
  std::set<DexCacheResolvedClasses> ret;
  for (auto&& pair : info_) {
    const std::string& profile_key = pair.first;
    const DexFileData& data = pair.second;
    // TODO: Is it OK to use the same location for both base and dex location here?
    DexCacheResolvedClasses classes(profile_key, profile_key, data.checksum);
    classes.AddClasses(data.class_set.begin(), data.class_set.end());
    ret.insert(classes);
  }
  return ret;
}

void ProfileCompilationInfo::ClearResolvedClasses() {
  for (auto& pair : info_) {
    pair.second.class_set.clear();
  }
}

// Naive implementation to generate a random profile file suitable for testing.
bool ProfileCompilationInfo::GenerateTestProfile(int fd,
                                                 uint16_t number_of_dex_files,
                                                 uint16_t method_ratio,
                                                 uint16_t class_ratio) {
  const std::string base_dex_location = "base.apk";
  ProfileCompilationInfo info;
  // The limits are defined by the dex specification.
  uint16_t max_method = std::numeric_limits<uint16_t>::max();
  uint16_t max_classes = std::numeric_limits<uint16_t>::max();
  uint16_t number_of_methods = max_method * method_ratio / 100;
  uint16_t number_of_classes = max_classes * class_ratio / 100;

  srand(MicroTime());

  // Make sure we generate more samples with a low index value.
  // This makes it more likely to hit valid method/class indices in small apps.
  const uint16_t kFavorFirstN = 10000;
  const uint16_t kFavorSplit = 2;

  for (uint16_t i = 0; i < number_of_dex_files; i++) {
    std::string dex_location = DexFile::GetMultiDexLocation(i, base_dex_location.c_str());
    std::string profile_key = GetProfileDexFileKey(dex_location);

    for (uint16_t m = 0; m < number_of_methods; m++) {
      uint16_t method_idx = rand() % max_method;
      if (m < (number_of_methods / kFavorSplit)) {
        method_idx %= kFavorFirstN;
      }
      info.AddMethodIndex(profile_key, 0, method_idx);
    }

    for (uint16_t c = 0; c < number_of_classes; c++) {
      uint16_t type_idx = rand() % max_classes;
      if (c < (number_of_classes / kFavorSplit)) {
        type_idx %= kFavorFirstN;
      }
      info.AddClassIndex(profile_key, 0, dex::TypeIndex(type_idx));
    }
  }
  return info.Save(fd);
}

}  // namespace art
