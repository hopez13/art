/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include "base/common_art_test.h"  // For ScratchFile
#include "base/file_utils.h"
#include "gtest/gtest.h"
#include "fd_file.h"
#include "random_access_file_test.h"

namespace unix_file {

class FdFileTest : public RandomAccessFileTest {
 protected:
  RandomAccessFile* MakeTestFile() override {
    FILE* tmp = tmpfile();
    int fd = art::DupCloexec(fileno(tmp));
    fclose(tmp);
    return new FdFile(fd, false);
  }
#ifdef __linux__
  // Variables and functions related to sparse copy tests.
  static constexpr int kNumChunks = 8;
  static constexpr size_t kChunkSize = 64 * art::KB;
  static constexpr size_t kStatBlockSize = 512;
  std::vector<int8_t> data_buffer_;
  std::vector<int8_t> zero_buffer_;
  std::vector<int8_t> check_buffer_;
  size_t fs_blocksize_;
  FdFileTest() {
    data_buffer_.resize(kChunkSize, 1);
    zero_buffer_.resize(kChunkSize);
    check_buffer_.resize(kChunkSize);
  }
  void CreateSparseSourceFile(std::unique_ptr<FdFile>& out_file,
                              size_t empty_prefix,
                              size_t empty_suffix,
                              off_t input_offset);
  void TestSparseCopiedData(FdFile* dest,
                            size_t empty_prefix,
                            size_t empty_suffix,
                            size_t copy_start_offset,
                            size_t copy_end_offset);
  size_t GetFilesystemBlockSize();
#endif
};

TEST_F(FdFileTest, Read) {
  TestRead();
}

TEST_F(FdFileTest, SetLength) {
  TestSetLength();
}

TEST_F(FdFileTest, Write) {
  TestWrite();
}

TEST_F(FdFileTest, UnopenedFile) {
  FdFile file;
  EXPECT_EQ(FdFile::kInvalidFd, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  EXPECT_TRUE(file.GetPath().empty());
}

TEST_F(FdFileTest, IsOpenFd) {
  art::ScratchFile scratch_file;
  FdFile* file = scratch_file.GetFile();
  ASSERT_TRUE(file->IsOpened());
  EXPECT_GE(file->Fd(), 0);
  EXPECT_NE(file->Fd(), FdFile::kInvalidFd);
  EXPECT_TRUE(FdFile::IsOpenFd(file->Fd()));
  int old_fd = file->Fd();
  ASSERT_TRUE(file != nullptr);
  ASSERT_EQ(file->FlushClose(), 0);
  EXPECT_FALSE(file->IsOpened());
  EXPECT_FALSE(FdFile::IsOpenFd(old_fd));
}

TEST_F(FdFileTest, OpenClose) {
  std::string good_path(GetTmpPath("some-file.txt"));
  FdFile file(good_path, O_CREAT | O_WRONLY, true);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_FALSE(file.ReadOnlyMode());
  EXPECT_EQ(0, file.Flush());
  EXPECT_EQ(0, file.Close());
  EXPECT_EQ(FdFile::kInvalidFd, file.Fd());
  EXPECT_FALSE(file.IsOpened());
  FdFile file2(good_path, O_RDONLY, true);
  EXPECT_TRUE(file2.IsOpened());
  EXPECT_TRUE(file2.ReadOnlyMode());
  EXPECT_GE(file2.Fd(), 0);

  ASSERT_EQ(file2.Close(), 0);
  ASSERT_EQ(unlink(good_path.c_str()), 0);
}

TEST_F(FdFileTest, ReadFullyEmptyFile) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDONLY, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_TRUE(file.ReadOnlyMode());
  EXPECT_GE(file.Fd(), 0);
  uint8_t buffer[16];
  EXPECT_FALSE(file.ReadFully(&buffer, 4));
}

template <size_t Size>
static void NullTerminateCharArray(char (&array)[Size]) {
  array[Size - 1] = '\0';
}

TEST_F(FdFileTest, ReadFullyWithOffset) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDWR, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);
  EXPECT_FALSE(file.ReadOnlyMode());

  char ignore_prefix[20] = {'a', };
  NullTerminateCharArray(ignore_prefix);
  char read_suffix[10] = {'b', };
  NullTerminateCharArray(read_suffix);

  off_t offset = 0;
  // Write scratch data to file that we can read back into.
  EXPECT_TRUE(file.Write(ignore_prefix, sizeof(ignore_prefix), offset));
  offset += sizeof(ignore_prefix);
  EXPECT_TRUE(file.Write(read_suffix, sizeof(read_suffix), offset));

  ASSERT_EQ(file.Flush(), 0);

  // Reading at an offset should only produce 'bbbb...', since we ignore the 'aaa...' prefix.
  char buffer[sizeof(read_suffix)];
  EXPECT_TRUE(file.PreadFully(buffer, sizeof(read_suffix), offset));
  EXPECT_STREQ(&read_suffix[0], &buffer[0]);

  ASSERT_EQ(file.Close(), 0);
}

TEST_F(FdFileTest, ReadWriteFullyWithOffset) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDWR, false);
  ASSERT_GE(file.Fd(), 0);
  EXPECT_TRUE(file.IsOpened());
  EXPECT_FALSE(file.ReadOnlyMode());

  const char* test_string = "This is a test string";
  size_t length = strlen(test_string) + 1;
  const size_t offset = 12;
  std::unique_ptr<char[]> offset_read_string(new char[length]);
  std::unique_ptr<char[]> read_string(new char[length]);

  // Write scratch data to file that we can read back into.
  EXPECT_TRUE(file.PwriteFully(test_string, length, offset));
  ASSERT_EQ(file.Flush(), 0);

  // Test reading both the offsets.
  EXPECT_TRUE(file.PreadFully(&offset_read_string[0], length, offset));
  EXPECT_STREQ(test_string, &offset_read_string[0]);

  EXPECT_TRUE(file.PreadFully(&read_string[0], length, 0u));
  EXPECT_NE(memcmp(&read_string[0], test_string, length), 0);

  ASSERT_EQ(file.Close(), 0);
}

// Create a sparse file and return a pointer to it via the 'out_file' argument, necessary because
// gtest assertions require the function to return void.
void FdFileTest::CreateSparseSourceFile(std::unique_ptr<FdFile>& out_file,
                                        size_t empty_prefix,
                                        size_t empty_suffix,
                                        off_t input_offset) {
/*
   * Layout of the source file:
   *   [ optional <input_offset> empty region ]
   *   [ optional <empty_prefix> empty region ]
   *   [ <kChunkSize> data chunk              ]  -\
   *   [ <kChunkSize> empty chunk             ]   |
   *   [ <kChunkSize> data chunk              ]   |
   *   [ <kChunkSize> empty chunk             ]    > (2 * kNumChunks - 1) kChunkSize chunks
   *   [ <kChunkSize> data chunk              ]   |
   *   [   ...                                ]   |
   *   [ <kChunkSize> data chunk              ]  -/
   *   [ optional <empty_suffix> empty region ]
   */

  // We return a new FdFile initialised from a temporary file generated via ScratchFile.
  // So keep the file on disk when the ScratchFile's destructor is called, in which case the FdFile
  // held by ScratchFile must explicitly marked as closed.
  art::ScratchFile src_tmp(/*keep_file=*/true);
  EXPECT_EQ(src_tmp.GetFile()->FlushClose(), 0);
  FdFile* src = new FdFile(src_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
  ASSERT_TRUE(src->IsOpened());

  ASSERT_EQ(lseek(src->Fd(), input_offset, SEEK_SET), input_offset);
  ASSERT_EQ(lseek(src->Fd(), empty_prefix, SEEK_CUR), (input_offset + empty_prefix));

  ASSERT_TRUE(src->WriteFully(data_buffer_.data(), kChunkSize));
  for (size_t i = 0; i < (kNumChunks -1); i++) {
    // Leave a chunk size of unwritten space between each data chunk.
    ASSERT_GT(lseek(src->Fd(), kChunkSize, SEEK_CUR), 0);
    ASSERT_TRUE(src->WriteFully(data_buffer_.data(), kChunkSize));
  }
  ASSERT_EQ(src->SetLength(src->GetLength() + empty_suffix), 0);
  ASSERT_EQ(src->Flush(), 0);

  size_t expected_length = (2 * kNumChunks - 1) * kChunkSize + empty_prefix + empty_suffix
                           + input_offset;
  ASSERT_EQ(src->GetLength(), expected_length);

  out_file.reset(src);
}

TEST_F(FdFileTest, Rename) {
  // To test that rename preserves sparsity (on systems that support file sparsity), create a sparse
  // source file.
  std::unique_ptr<FdFile> src;
  ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src, 0, 0, 0));

  size_t src_offset = lseek(src->Fd(), 0, SEEK_CUR);
  size_t source_length = src->GetLength();
  struct stat src_stat;
  ASSERT_EQ(fstat(src->Fd(), &src_stat), 0);

  // Move the file via a rename.
  art::ScratchFile dest_tmp;
  std::string new_filename = dest_tmp.GetFilename();
  std::string old_filename = src->GetPath();
  ASSERT_TRUE(src->Rename(new_filename));

  // Confirm the FdFile path has correctly updated.
  EXPECT_EQ(src->GetPath(), new_filename);
  // Check the offset of the moved file has not been modified.
  EXPECT_EQ(lseek(src->Fd(), 0, SEEK_CUR), src_offset);

  ASSERT_EQ(src->Close(), 0);

  // Test that the file no longer exists in the old location, and there is a file at the new
  // location with the expected length.
  EXPECT_FALSE(art::OS::FileExists(old_filename.c_str()));
  FdFile dest(new_filename, O_RDONLY, /*check_usage=*/false);
  ASSERT_TRUE(dest.IsOpened());
  EXPECT_EQ(dest.GetLength(), source_length);

  // Confirm the file at the new location has the same number of allocated data blocks as the source
  // file. If the source file was a sparse file, this confirms that the sparsity was preserved
  // by the move.
  struct stat dest_stat;
  ASSERT_EQ(fstat(dest.Fd(), &dest_stat), 0);
  EXPECT_EQ(dest_stat.st_blocks, src_stat.st_blocks);

  // And it is exactly the same file in the new location, with the same contents.
  EXPECT_EQ(dest_stat.st_dev, src_stat.st_dev);
  EXPECT_EQ(dest_stat.st_ino, src_stat.st_ino);
  ASSERT_NO_FATAL_FAILURE(TestSparseCopiedData(&dest, 0, 0, 0, 0));

  EXPECT_EQ(dest.Close(), 0);
}

TEST_F(FdFileTest, Copy) {
  art::ScratchFile src_tmp;
  FdFile src(src_tmp.GetFilename(), O_RDWR, false);
  ASSERT_GE(src.Fd(), 0);
  ASSERT_TRUE(src.IsOpened());

  char src_data[] = "Some test data.";
  ASSERT_TRUE(src.WriteFully(src_data, sizeof(src_data)));  // Including the zero terminator.
  ASSERT_EQ(0, src.Flush());
  ASSERT_EQ(static_cast<int64_t>(sizeof(src_data)), src.GetLength());

  art::ScratchFile dest_tmp;
  FdFile dest(dest_tmp.GetFilename(), O_RDWR, false);
  ASSERT_GE(dest.Fd(), 0);
  ASSERT_TRUE(dest.IsOpened());

  ASSERT_TRUE(dest.Copy(&src, 0, sizeof(src_data)));
  ASSERT_EQ(0, dest.Flush());
  ASSERT_EQ(static_cast<int64_t>(sizeof(src_data)), dest.GetLength());

  char check_data[sizeof(src_data)];
  ASSERT_TRUE(dest.PreadFully(check_data, sizeof(src_data), 0u));
  CHECK_EQ(0, memcmp(check_data, src_data, sizeof(src_data)));

  ASSERT_EQ(0, dest.Close());
  ASSERT_EQ(0, src.Close());
}

#ifdef __linux__
// Helper to assert correctness of the data copied to a destination file based on a source sparse
// file created via FdFileTest::CreateSparseSourceFile.
void FdFileTest::TestSparseCopiedData(FdFile* dest,
                                      size_t empty_prefix,
                                      size_t empty_suffix,
                                      size_t copy_start_offset,
                                      size_t copy_end_offset) {
  // For testing partial copies of the source file, when 'copy_start_offset' or 'copy_end_offset'
  // are non-zero, we further reduce the size of the regions we should expect in the output,
  // calculated as follows.
  size_t first_hole_size = 0;
  size_t first_data_size = kChunkSize;
  if (copy_start_offset > empty_prefix) {
    first_hole_size = 0;
    first_data_size -= (copy_start_offset - empty_prefix);
  } else {
    first_hole_size = empty_prefix - copy_start_offset;
  }
  size_t last_hole_size = 0;
  size_t last_data_size = kChunkSize;
  if (copy_end_offset > empty_suffix) {
    last_hole_size = 0;
    last_data_size -= (copy_end_offset - empty_suffix);
  } else {
    last_hole_size = empty_suffix - copy_end_offset;
  }

  ASSERT_TRUE(dest->ReadFully(check_buffer_.data(), first_hole_size));
  EXPECT_EQ(0, memcmp(check_buffer_.data(), zero_buffer_.data(), first_hole_size));
  ASSERT_TRUE(dest->ReadFully(check_buffer_.data(), first_data_size));
  EXPECT_EQ(0, memcmp(check_buffer_.data(), data_buffer_.data(), first_data_size));

  for (size_t i = 1; i < 2 * kNumChunks - 2; i++) {
    ASSERT_TRUE(dest->ReadFully(check_buffer_.data(), kChunkSize));
    if (i % 2 == 0) {
      EXPECT_EQ(0, memcmp(check_buffer_.data(), data_buffer_.data(), kChunkSize));
    } else {
      EXPECT_EQ(0, memcmp(check_buffer_.data(), zero_buffer_.data(), kChunkSize));
    }
  }

  ASSERT_TRUE(dest->ReadFully(check_buffer_.data(), last_data_size));
  EXPECT_EQ(0, memcmp(check_buffer_.data(), data_buffer_.data(), last_data_size));
  ASSERT_TRUE(dest->ReadFully(check_buffer_.data(), last_hole_size));
  EXPECT_EQ(0, memcmp(check_buffer_.data(), zero_buffer_.data(), last_hole_size));
}

// Test that the file created by FdFileTest::CreateSparseSourceFile is sparse on the test
// environment.
TEST_F(FdFileTest, CopySparse_CreateSparseFile) {
  // Create file with no empty prefix or suffix, and no offset.
  std::unique_ptr<FdFile> src1;
  ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src1, 0, 0, 0));

  struct stat src1_stat;
  ASSERT_EQ(fstat(src1->Fd(), &src1_stat), 0);
  EXPECT_GE(src1_stat.st_blocks, kNumChunks * kChunkSize / kStatBlockSize);
  EXPECT_LT(src1_stat.st_blocks * kStatBlockSize, src1_stat.st_size);

  // Create file with a prefix region, suffix region, and an offset.
  std::unique_ptr<FdFile> src2;
  ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src2, kChunkSize, kChunkSize, kChunkSize));

  // File should have the same number of allocated blocks.
  struct stat src2_stat;
  ASSERT_EQ(fstat(src2->Fd(), &src2_stat), 0);
  EXPECT_EQ(src2_stat.st_blocks, src1_stat.st_blocks);

  EXPECT_TRUE(src1->Erase(/*unlink=*/true));
  EXPECT_TRUE(src2->Erase(/*unlink=*/true));
}

// Test complete copies of the source file produced by FdFileTest::CreateSparseSourceFile.
TEST_F(FdFileTest, CopySparse_FullCopy) {
  auto verify_fullcopy = [&](size_t empty_prefix, size_t empty_suffix, size_t offset) {
    SCOPED_TRACE(testing::Message() << "prefix:" << empty_prefix << ", suffix:" << empty_suffix
                 << ", offset:" << offset);

    std::unique_ptr<FdFile> src;
    ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src, empty_prefix, empty_suffix, offset));

    art::ScratchFile dest_tmp;
    FdFile dest(dest_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
    ASSERT_TRUE(dest.IsOpened());

    off_t copy_size = src->GetLength() - offset;
    EXPECT_TRUE(dest.Copy(src.get(), offset, copy_size));
    EXPECT_EQ(dest.Flush(), 0);

    // Test destination length.
    EXPECT_EQ(dest.GetLength(), copy_size);

    // Test FD offsets.
    EXPECT_EQ(lseek(dest.Fd(), 0, SEEK_CUR), dest.GetLength());
    EXPECT_EQ(lseek(src->Fd(), 0, SEEK_CUR), src->GetLength());

    // Test same number of allocated blocks for this full copy.
    struct stat src_stat, dest_stat;
    ASSERT_EQ(fstat(src->Fd(), &src_stat), 0);
    ASSERT_EQ(fstat(dest.Fd(), &dest_stat), 0);
    EXPECT_EQ(dest_stat.st_blocks, src_stat.st_blocks);

    // Test the resulting data in the destination is correct.
    ASSERT_EQ(lseek(dest.Fd(), 0, SEEK_SET), 0);
    ASSERT_NO_FATAL_FAILURE(TestSparseCopiedData(&dest, empty_prefix, empty_suffix, 0, 0));

    EXPECT_TRUE(src->Erase(/*unlink=*/true));
  };

  // Test full copies using different offsets and outer skip regions of sizes [0, 128, 2048, 32768].
  ASSERT_NO_FATAL_FAILURE(verify_fullcopy(0, 0, 0));
  ASSERT_NO_FATAL_FAILURE(verify_fullcopy(0, 0, kChunkSize/2));
  for (size_t empty_region_size=128; empty_region_size <= kChunkSize/2; empty_region_size <<= 4) {
    // Empty prefix.
    ASSERT_NO_FATAL_FAILURE(verify_fullcopy(empty_region_size, 0, 0));
    ASSERT_NO_FATAL_FAILURE(verify_fullcopy(empty_region_size, 0, kChunkSize/2));
    // Empty suffix.
    ASSERT_NO_FATAL_FAILURE(verify_fullcopy(0, empty_region_size, 0));
    ASSERT_NO_FATAL_FAILURE(verify_fullcopy(0, empty_region_size, kChunkSize/2));
    // Both.
    ASSERT_NO_FATAL_FAILURE(verify_fullcopy(empty_region_size, empty_region_size, 0));
    ASSERT_NO_FATAL_FAILURE(verify_fullcopy(empty_region_size, empty_region_size, kChunkSize/2));
  }
}

// Test partial copies of the source file produced by FdFileTest::CreateSparseSourceFile. Depending
// on the layout of the source file (as controlled by empty_prefix, empy_suffix, and input_offset),
// partial copies may copy less data bytes, less zeroed 'hole' bytes, or both.
TEST_F(FdFileTest, CopySparse_PartialCopy) {
  auto verify_partialcopy = [&](size_t empty_prefix,
                                size_t empty_suffix,
                                size_t offset,
                                size_t copy_start_offset,
                                size_t copy_end_offset) {
    // The file is copied starting from <input_offset> + <copy_start_offset>.
    // The copy ends <copy_end_offset> from the end of the source file.
    SCOPED_TRACE(testing::Message() << "prefix:" << empty_prefix << ", suffix:" << empty_suffix
                 << ", offset:" << offset << ", copy_start_offset:" << copy_start_offset
                 << ", copy_end_offset:" << copy_end_offset);

    // For simplicity, do not discard more than one chunk from the start or end of the source file.
    ASSERT_LE(copy_start_offset + empty_prefix, kChunkSize);
    ASSERT_LE(copy_end_offset + empty_suffix, kChunkSize);

    std::unique_ptr<FdFile> src;
    ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src, empty_prefix, empty_suffix, offset));

    art::ScratchFile dest_tmp;
    FdFile dest(dest_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
    ASSERT_TRUE(dest.IsOpened());

    off_t copy_size = src->GetLength() - offset - copy_start_offset - copy_end_offset;
    EXPECT_TRUE(dest.Copy(src.get(), offset + copy_start_offset, copy_size));
    EXPECT_EQ(dest.Flush(), 0);

    // Test destination length.
    EXPECT_EQ(dest.GetLength(), copy_size);

    // Test FD offsets.
    EXPECT_EQ(lseek(dest.Fd(), 0, SEEK_CUR), dest.GetLength());
    EXPECT_EQ(lseek(src->Fd(), 0, SEEK_CUR), src->GetLength() - copy_end_offset);

    // Test the resulting data in the destination is correct.
    ASSERT_EQ(lseek(dest.Fd(), 0, SEEK_SET), 0);
    ASSERT_NO_FATAL_FAILURE(TestSparseCopiedData(&dest,
                                                 empty_prefix,
                                                 empty_suffix,
                                                 copy_start_offset,
                                                 copy_end_offset));

    EXPECT_TRUE(src->Erase(/*unlink=*/true));
  };

  // Test partial copies with outer skip regions.
  std::vector<size_t> outer_regions = {0, 128, 2 * art::KB, 32 * art::KB};
  for (const auto& empty : outer_regions) {
    for (size_t discard=0; discard <= 8 * art::KB; discard += 1 * art::KB) {
      // Start copy after the data section start.
      ASSERT_NO_FATAL_FAILURE(verify_partialcopy(empty, empty, empty, discard, 0));
      // End copy before the file end.
      ASSERT_NO_FATAL_FAILURE(verify_partialcopy(empty, empty, empty, 0, discard));
      // Both.
      ASSERT_NO_FATAL_FAILURE(verify_partialcopy(empty, empty, empty, discard, discard));
    }
  }
}

// Helper function to calculate the number of fstat blocks (st_blocks) discarded by a partial copy.
size_t CalculateNumDiscardedFStatBlocks(size_t empty_prefix,
                                        size_t empty_suffix,
                                        size_t copy_start_offset,
                                        size_t copy_end_offset,
                                        size_t fs_blocksize,
                                        size_t fstat_blocksize) {
  // If the start/end is an empty prefix/suffix region, we might not be discarding any data.
  size_t discard_start = copy_start_offset > empty_prefix ? copy_start_offset - empty_prefix : 0;
  size_t discard_end = copy_end_offset > empty_suffix ? copy_end_offset - empty_suffix : 0;
  // Each range gets rounded down to whole filesystem blocks that are discarded, which then need
  // to be converted to fstat's block size.
  size_t discarded_blocks = (discard_start / fs_blocksize) + (discard_end / fs_blocksize);
  discarded_blocks *= (fs_blocksize / fstat_blocksize);
  return discarded_blocks;
}

// Find the filesystem blocksize of the test environment by creating and calling fstat on a
// temporary file.
size_t FdFileTest::GetFilesystemBlockSize() {
  if (fs_blocksize_ == 0) {
    art::ScratchFile tmpfile;
    FdFile tmp(tmpfile.GetFilename(), O_RDWR, /*check_usage=*/false);
    if (!tmp.IsOpened()) {
      return 0;
    }
    struct stat tmp_stat;
    if (fstat(tmp.Fd(), &tmp_stat) != 0) {
      return 0;
    }
    fs_blocksize_ = tmp_stat.st_blksize;
  }
  return fs_blocksize_;
}

// Test the copy function's requirement that only copies which are aligned with the filesystem
// blocksize will preserve the source file's sparsity.
TEST_F(FdFileTest, CopySparse_TestAlignment) {
  size_t fs_blocksize = GetFilesystemBlockSize();
  ASSERT_NE(fs_blocksize, 0);

  auto verify_partialcopy = [&](size_t empty_prefix,
                                size_t empty_suffix,
                                size_t offset,
                                size_t copy_start_offset,
                                size_t copy_end_offset) {
    SCOPED_TRACE(testing::Message() << "prefix:" << empty_prefix << ", suffix:" << empty_suffix
                 << ", offset:" << offset << ", copy_start_offset:" << copy_start_offset
                 << ", copy_end_offset:" << copy_end_offset);

    // For simplicity, do not discard more than one chunk from the start or end of the source file.
    ASSERT_LE(copy_start_offset + empty_prefix, kChunkSize);
    ASSERT_LE(copy_end_offset + empty_suffix, kChunkSize);
    // Only reason about the expected sparsity if the source data created in this test is aligned
    // with the filesystem bock size. Otherwise, varying the offset could increase, decrease, or
    // preserve the sparsity depending on data layout, which would complicate the test logic.
    ASSERT_EQ((offset + empty_prefix) % fs_blocksize, 0);

    std::unique_ptr<FdFile> src;
    ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src, empty_prefix, empty_suffix, offset));

    art::ScratchFile dest_tmp;
    FdFile dest(dest_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
    ASSERT_TRUE(dest.IsOpened());

    off_t copy_size = src->GetLength() - offset - copy_start_offset - copy_end_offset;
    EXPECT_TRUE(dest.Copy(src.get(), offset + copy_start_offset, copy_size));
    EXPECT_EQ(dest.Flush(), 0);

    // Test the alignment has the expected effect on the file sparsity after accounting for any data
    // we didn't copy.
    size_t discarded_blocks = CalculateNumDiscardedFStatBlocks(empty_prefix,
                                                               empty_suffix,
                                                               copy_start_offset,
                                                               copy_end_offset,
                                                               fs_blocksize,
                                                               kStatBlockSize);
    struct stat src_stat, dest_stat;
    ASSERT_EQ(fstat(src->Fd(), &src_stat), 0);
    ASSERT_EQ(fstat(dest.Fd(), &dest_stat), 0);

    if ((offset + copy_start_offset) % fs_blocksize == 0) {
      // If the start of the copy is aligned with the filesystem block size, then we expect the
      // sparsity to be preserved.
      EXPECT_EQ(dest_stat.st_blocks, src_stat.st_blocks - discarded_blocks);
    } else {
      // As we know that all data chunks are aligned, any non-aligned copy can only cause holes
      // in the source file to be created as allocated data blocks in the output file.
      EXPECT_GT(dest_stat.st_blocks, src_stat.st_blocks - discarded_blocks);
    }

    EXPECT_TRUE(src->Erase(/*unlink=*/true));
  };

  // Start the copy at different offsets relative to the data, moving in and out of alignment.
  for (size_t discard=0; discard <= 2 * fs_blocksize; discard += 1 * art::KB) {
    ASSERT_NO_FATAL_FAILURE(verify_partialcopy(0, 0, 0, discard, 0));
    // Add an empty prefix and input offset that results in source file data remaining aligned.
    ASSERT_NO_FATAL_FAILURE(verify_partialcopy(fs_blocksize/2, 0, fs_blocksize/2, discard, 0));
  }
}

// Test the case where the destination file's FD offset is non-zero before the copy.
TEST_F(FdFileTest, CopySparse_ToNonZeroOffset) {
  constexpr size_t existing_datasize = kChunkSize;
  constexpr size_t existing_holesize = kChunkSize;

  std::unique_ptr<FdFile> src;
  ASSERT_NO_FATAL_FAILURE(CreateSparseSourceFile(src, 0, 0, 0));

  art::ScratchFile dest_tmp;
  FdFile dest(dest_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
  ASSERT_TRUE(dest.IsOpened());

  // Modify the output file's FD offset by writing some data and lseeking to create a hole.
  size_t existing_length = existing_datasize + existing_holesize;
  ASSERT_TRUE(dest.WriteFully(data_buffer_.data(), existing_datasize));
  EXPECT_EQ(lseek(dest.Fd(), existing_holesize, SEEK_CUR), existing_length);
  ASSERT_EQ(0, dest.SetLength(existing_length));

  off_t copy_size = src->GetLength();
  EXPECT_TRUE(dest.Copy(src.get(), 0, copy_size));
  EXPECT_EQ(dest.Flush(), 0);

  // Test destination length.
  EXPECT_EQ(dest.GetLength(), existing_length + copy_size);

  // Test FD offsets.
  EXPECT_EQ(lseek(dest.Fd(), 0, SEEK_CUR), dest.GetLength());
  EXPECT_EQ(lseek(src->Fd(), 0, SEEK_CUR), src->GetLength());

  // Test the copied data in the destination is correct, ignoring the first 'existing_length'.
  ASSERT_EQ(lseek(dest.Fd(), existing_length, SEEK_SET), existing_length);
  ASSERT_NO_FATAL_FAILURE(TestSparseCopiedData(&dest, 0, 0, 0, 0));

  EXPECT_TRUE(src->Erase(/*unlink=*/true));
}
#endif

TEST_F(FdFileTest, MoveConstructor) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDWR, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);

  int old_fd = file.Fd();

  FdFile file2(std::move(file));
  EXPECT_FALSE(file.IsOpened());  // NOLINT - checking file is no longer opened after move
  EXPECT_TRUE(file2.IsOpened());
  EXPECT_EQ(old_fd, file2.Fd());

  ASSERT_EQ(file2.Flush(), 0);
  ASSERT_EQ(file2.Close(), 0);
}

TEST_F(FdFileTest, OperatorMoveEquals) {
  // Make sure the read_only_ flag is correctly copied
  // over.
  art::ScratchFile tmp;
  FdFile file(tmp.GetFilename(), O_RDONLY, false);
  ASSERT_TRUE(file.ReadOnlyMode());

  FdFile file2(tmp.GetFilename(), O_RDWR, false);
  ASSERT_FALSE(file2.ReadOnlyMode());

  file2 = std::move(file);
  ASSERT_TRUE(file2.ReadOnlyMode());
}

TEST_F(FdFileTest, EraseWithPathUnlinks) {
  // New scratch file, zero-length.
  art::ScratchFile tmp;
  std::string filename = tmp.GetFilename();
  tmp.Close();  // This is required because of the unlink race between the scratch file and the
                // FdFile, which leads to close-guard breakage.
  FdFile file(filename, O_RDWR, false);
  ASSERT_TRUE(file.IsOpened());
  EXPECT_GE(file.Fd(), 0);
  uint8_t buffer[16] = { 0 };
  EXPECT_TRUE(file.WriteFully(&buffer, sizeof(buffer)));
  EXPECT_EQ(file.Flush(), 0);

  EXPECT_TRUE(file.Erase(true));

  EXPECT_FALSE(file.IsOpened());

  EXPECT_FALSE(art::OS::FileExists(filename.c_str())) << filename;
}

TEST_F(FdFileTest, Compare) {
  std::vector<uint8_t> buffer;
  constexpr int64_t length = 17 * art::KB;
  for (size_t i = 0; i < length; ++i) {
    buffer.push_back(static_cast<uint8_t>(i));
  }

  auto reset_compare = [&](art::ScratchFile& a, art::ScratchFile& b) {
    a.GetFile()->ResetOffset();
    b.GetFile()->ResetOffset();
    return a.GetFile()->Compare(b.GetFile());
  };

  art::ScratchFile tmp;
  EXPECT_TRUE(tmp.GetFile()->WriteFully(&buffer[0], length));
  EXPECT_EQ(tmp.GetFile()->GetLength(), length);

  art::ScratchFile tmp2;
  EXPECT_TRUE(tmp2.GetFile()->WriteFully(&buffer[0], length));
  EXPECT_EQ(tmp2.GetFile()->GetLength(), length);

  // Basic equality check.
  tmp.GetFile()->ResetOffset();
  tmp2.GetFile()->ResetOffset();
  // Files should be the same.
  EXPECT_EQ(reset_compare(tmp, tmp2), 0);

  // Change a byte near the start.
  ++buffer[2];
  art::ScratchFile tmp3;
  EXPECT_TRUE(tmp3.GetFile()->WriteFully(&buffer[0], length));
  --buffer[2];
  EXPECT_NE(reset_compare(tmp, tmp3), 0);

  // Change a byte near the middle.
  ++buffer[length / 2];
  art::ScratchFile tmp4;
  EXPECT_TRUE(tmp4.GetFile()->WriteFully(&buffer[0], length));
  --buffer[length / 2];
  EXPECT_NE(reset_compare(tmp, tmp4), 0);

  // Change a byte near the end.
  ++buffer[length - 5];
  art::ScratchFile tmp5;
  EXPECT_TRUE(tmp5.GetFile()->WriteFully(&buffer[0], length));
  --buffer[length - 5];
  EXPECT_NE(reset_compare(tmp, tmp5), 0);

  // Reference check
  art::ScratchFile tmp6;
  EXPECT_TRUE(tmp6.GetFile()->WriteFully(&buffer[0], length));
  EXPECT_EQ(reset_compare(tmp, tmp6), 0);
}

TEST_F(FdFileTest, PipeFlush) {
  int pipefd[2];
  ASSERT_EQ(0, pipe2(pipefd, O_CLOEXEC));

  FdFile file(pipefd[1], true);
  ASSERT_TRUE(file.WriteFully("foo", 3));
  ASSERT_EQ(0, file.Flush());
  ASSERT_EQ(0, file.FlushCloseOrErase());
  close(pipefd[0]);
}

}  // namespace unix_file
