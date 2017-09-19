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

#include <sys/mman.h>

#include "common_runtime_test.h"
#include "dex_file-inl.h"
#include "gtest/gtest.h"
#include "mem_map.h"
#include "safe_dex_file.h"

namespace art {

class SafeDexFileTest : public CommonRuntimeTest {
 public:
  SafeDexFileTest() : dex_file_(), writeable_page_(nullptr), unreadable_page_(nullptr) { }

 protected:
  bool OpenTestDexFile(std::string* error_msg) {
    MemMap::Init();
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    std::string location = GetTestDexFileName("Main");
    if (!DexFile::Open(location.c_str(),
                       location,
                       /* verify_checksum */ true,
                       error_msg,
                       &dex_files)) {
      return false;
    }
    if (dex_files.empty()) {
      *error_msg = "No dex files in " + location;
      return false;
    }

    const DexFile* src_dex_file = dex_files[0].get();
    const size_t src_dex_file_size = src_dex_file->Size();
    size_t map_size = RoundUp(src_dex_file_size, kPageSize) + 2 * kPageSize;
    std::unique_ptr<MemMap> data(MemMap::MapAnonymous("TestDexFile",
                                                      /* addr */ nullptr,
                                                      map_size,
                                                      PROT_READ | PROT_WRITE,
                                                      /* low_4gb */ false,
                                                      /* reuse */ false,
                                                      error_msg));
    if (data == nullptr) {
      return false;
    }

    memcpy(data->Begin(), src_dex_file->Begin(), src_dex_file_size);
    // Keep the dex file writeable, so that we can hack offsets to point wherever we want.
    uint8_t* writeable_page = data->Begin() + RoundUp(src_dex_file_size, kPageSize);
    uint8_t* unreadable_page = writeable_page + kPageSize;
    mprotect(unreadable_page, kPageSize, PROT_NONE);
    dex_files.clear();
    std::unique_ptr<const DexFile> dex_file = DexFile::Open(data->Begin(),
                                                            src_dex_file_size,
                                                            "TestDexFile",
                                                            /* location_checksum */ 0u,
                                                            /* oat_dex_file */ nullptr,
                                                            /* verify */ true,
                                                            /* verify_checksum */ false,
                                                            error_msg);
    if (dex_file == nullptr) {
      return false;
    }

    data_ = std::move(data);
    dex_file_ = std::move(dex_file);
    writeable_page_ = writeable_page;
    unreadable_page_ = unreadable_page;
    return true;
  }

  uint32_t StoreAtEndOfWriteablePage(const uint8_t* src, size_t length) {
    CHECK_LT(length, static_cast<size_t>(kPageSize));
    CHECK(writeable_page_ != nullptr);
    uint8_t* dest = writeable_page_ + kPageSize - length;
    memcpy(dest, src, length);
    return dest - dex_file_->Begin();
  }

  uint32_t StoreAtEndOfWriteablePage(const char* src, size_t length) {
    return StoreAtEndOfWriteablePage(reinterpret_cast<const uint8_t*>(src), length);
  }

  std::unique_ptr<MemMap> data_;
  std::unique_ptr<const DexFile> dex_file_;
  uint8_t* writeable_page_;
  uint8_t* unreadable_page_;
};

TEST_F(SafeDexFileTest, String) {
  std::string error_msg;
  ASSERT_TRUE(OpenTestDexFile(&error_msg)) << error_msg;
  const DexFile::StringId* main_sid = dex_file_->FindStringId("LMain;");
  ASSERT_TRUE(main_sid != nullptr);
  dex::StringIndex main_string_index = dex_file_->GetIndexForStringId(*main_sid);
  ASSERT_EQ("LMain;", SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test unreadable DexFile or dex file header.
  mprotect(const_cast<uint8_t*>(dex_file_->Begin()), kPageSize, PROT_NONE);
  const DexFile* unreadable_dex_file = reinterpret_cast<const DexFile*>(unreadable_page_);
  EXPECT_EQ("StringInUnreadableDexFile",
            SafeDexFile::String(unreadable_dex_file, main_string_index));
  EXPECT_EQ("StringInUnreadableDexFile", SafeDexFile::String(dex_file_.get(), main_string_index));
  // Invalid string index yields "" even for unreadable DexFile or DexFile header.
  EXPECT_EQ("", SafeDexFile::String(unreadable_dex_file, dex::StringIndex(DexFile::kDexNoIndex)));
  EXPECT_EQ("", SafeDexFile::String(dex_file_.get(), dex::StringIndex(DexFile::kDexNoIndex)));
  mprotect(const_cast<uint8_t*>(dex_file_->Begin()), kPageSize, PROT_READ | PROT_WRITE);

  // Test invalid large index.
  constexpr uint32_t kLargeIndex = 1000000u;  // The dex file should not have that many strings.
  ASSERT_GE(kLargeIndex, dex_file_->NumStringIds());
  EXPECT_EQ("StringOOB#" + std::to_string(kLargeIndex) +
                "/" + std::to_string(dex_file_->NumStringIds()),
            SafeDexFile::String(dex_file_.get(), dex::StringIndex(kLargeIndex)));

  // Test unreadable StringId.
  const DexFile::Header* header = &dex_file_->GetHeader();
  uint32_t original_string_ids_offset = header->string_ids_off_;
  const_cast<DexFile::Header*>(header)->string_ids_off_ =
      unreadable_page_ - dex_file_->Begin() - main_string_index.index_ * sizeof(DexFile::StringId);
  EXPECT_EQ("StringWithUnreadableId#" + std::to_string(main_string_index.index_),
            SafeDexFile::String(dex_file_.get(), main_string_index));
  const_cast<DexFile::Header*>(header)->string_ids_off_ = original_string_ids_offset;

  // Test unreadable terminating Nul.
  const char kTestUnreadableNul[] = "\015UnreadableNul";
  CHECK_EQ(static_cast<size_t>(kTestUnreadableNul[0]), sizeof(kTestUnreadableNul) - 2u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kTestUnreadableNul, sizeof(kTestUnreadableNul) - 1);
  ASSERT_EQ("UnreadableNul[UNREADABLE-NUL]",
            SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test string length being fully in unreadable.
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      unreadable_page_ - dex_file_->Begin();
  ASSERT_EQ("StringWithUnreadableLength#" + std::to_string(main_string_index.index_),
            SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test string length being partially unreadable, i.e. unterminated LEB128 number.
  const char kPartialLength[] = "\x81";
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kPartialLength, 1u);
  ASSERT_EQ("StringWithUnreadableLength#" + std::to_string(main_string_index.index_),
            SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test missing terminating Nul, i.e. the string continues beyond declared length.
  const char kMissingNul[] = "\x0aMissingNulX";
  CHECK_EQ(static_cast<size_t>(kMissingNul[0]), sizeof(kMissingNul) - 3u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kMissingNul, sizeof(kMissingNul) - 1);
  ASSERT_EQ("MissingNul[MISSING-NUL]", SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test unterminated string, i.e. the string's declared length reaches unreadable memory.
  const char kUnterminated[] = "\x0dUnterminated";
  CHECK_EQ(static_cast<size_t>(kUnterminated[0]), sizeof(kUnterminated) - 1u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kUnterminated, sizeof(kUnterminated) - 1);
  ASSERT_EQ("Unterminated[...]", SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test string with unreadable memory starting in the middle of a multi-byte character.
  const char kUnterminated2[] = "\x0dUnterminate\xca";  // Missing continuation character.
  CHECK_EQ(static_cast<size_t>(kUnterminated2[0]), sizeof(kUnterminated2) - 1u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kUnterminated2, sizeof(kUnterminated2) - 1);
  ASSERT_EQ("Unterminate[...]", SafeDexFile::String(dex_file_.get(), main_string_index));

  // Test broken string where surrogate pair crosses the string length.
  const char kBroken[] = "\x07" "Broken\xf1\x81\x81\x81";
  CHECK_EQ(static_cast<size_t>(kBroken[0]), sizeof(kBroken) - 5u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kBroken, sizeof(kBroken) - 1);
  ASSERT_EQ("Broken[BROKEN]", SafeDexFile::String(dex_file_.get(), main_string_index));
}

TEST_F(SafeDexFileTest, Type) {
  std::string error_msg;
  ASSERT_TRUE(OpenTestDexFile(&error_msg)) << error_msg;
  const DexFile::StringId* main_sid = dex_file_->FindStringId("LMain;");
  ASSERT_TRUE(main_sid != nullptr);
  dex::StringIndex main_string_index = dex_file_->GetIndexForStringId(*main_sid);
  const DexFile::TypeId* main_tid = dex_file_->FindTypeId(main_string_index);
  ASSERT_TRUE(main_tid != nullptr);
  dex::TypeIndex main_type_index = dex_file_->GetIndexForTypeId(*main_tid);
  ASSERT_EQ("LMain;", SafeDexFile::Descriptor(dex_file_.get(), main_type_index));
  ASSERT_EQ("Main", SafeDexFile::PrettyDescriptor(dex_file_.get(), main_type_index));

  // Test unreadable DexFile or dex file header.
  mprotect(const_cast<uint8_t*>(dex_file_->Begin()), kPageSize, PROT_NONE);
  const DexFile* unreadable_dex_file = reinterpret_cast<const DexFile*>(unreadable_page_);
  EXPECT_EQ("TypeInUnreadableDexFile",
            SafeDexFile::Descriptor(unreadable_dex_file, main_type_index));
  EXPECT_EQ("TypeInUnreadableDexFile",
            SafeDexFile::PrettyDescriptor(unreadable_dex_file, main_type_index));
  EXPECT_EQ("TypeInUnreadableDexFile", SafeDexFile::Descriptor(dex_file_.get(), main_type_index));
  EXPECT_EQ("TypeInUnreadableDexFile",
            SafeDexFile::PrettyDescriptor(dex_file_.get(), main_type_index));
  mprotect(const_cast<uint8_t*>(dex_file_->Begin()), kPageSize, PROT_READ | PROT_WRITE);

  // Test invalid large index.
  constexpr uint16_t kLargeIndex = 60000u;  // The dex file should not have that many types.
  ASSERT_GE(kLargeIndex, dex_file_->NumTypeIds());
  EXPECT_EQ("TypeOOB#" + std::to_string(kLargeIndex) +
                "/" + std::to_string(dex_file_->NumTypeIds()),
            SafeDexFile::Descriptor(dex_file_.get(), dex::TypeIndex(kLargeIndex)));
  EXPECT_EQ("TypeOOB#" + std::to_string(kLargeIndex) +
                "/" + std::to_string(dex_file_->NumTypeIds()),
            SafeDexFile::PrettyDescriptor(dex_file_.get(), dex::TypeIndex(kLargeIndex)));

  // Test unreadable TypeId.
  const DexFile::Header* header = &dex_file_->GetHeader();
  uint32_t original_type_ids_offset = header->type_ids_off_;
  const_cast<DexFile::Header*>(header)->type_ids_off_ =
      unreadable_page_ - dex_file_->Begin() - main_type_index.index_ * sizeof(DexFile::TypeId);
  EXPECT_EQ("TypeWithUnreadableId#" + std::to_string(main_type_index.index_),
            SafeDexFile::Descriptor(dex_file_.get(), main_type_index));
  EXPECT_EQ("TypeWithUnreadableId#" + std::to_string(main_type_index.index_),
            SafeDexFile::PrettyDescriptor(dex_file_.get(), main_type_index));
  const_cast<DexFile::Header*>(header)->type_ids_off_ = original_type_ids_offset;

  // Test fully readable descriptor with package specifier.
  const char kPkgSomeClass[] = "\x0fLpkg/SomeClass;";
  CHECK_EQ(static_cast<size_t>(kPkgSomeClass[0]), sizeof(kPkgSomeClass) - 2u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kPkgSomeClass, sizeof(kPkgSomeClass));
  ASSERT_EQ("Lpkg/SomeClass;", SafeDexFile::Descriptor(dex_file_.get(), main_type_index));
  ASSERT_EQ("pkg.SomeClass", SafeDexFile::PrettyDescriptor(dex_file_.get(), main_type_index));

  // Test partially readable descriptor with package specifier.
  const char kPkgSomeClassTruncated[] = "\x0fLpkg/SomeClass";
  CHECK_EQ(static_cast<size_t>(kPkgSomeClassTruncated[0]), sizeof(kPkgSomeClassTruncated) - 1u);
  const_cast<DexFile::StringId*>(main_sid)->string_data_off_ =
      StoreAtEndOfWriteablePage(kPkgSomeClassTruncated, sizeof(kPkgSomeClassTruncated) - 1u);
  ASSERT_EQ("Lpkg/SomeClass[...]", SafeDexFile::Descriptor(dex_file_.get(), main_type_index));
  ASSERT_EQ("pkg.SomeClass[...]", SafeDexFile::PrettyDescriptor(dex_file_.get(), main_type_index));
}

}  // namespace art
