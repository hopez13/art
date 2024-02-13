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
TEST_F(FdFileTest, CopySparse) {
  // The test validates that FdFile::Copy preserves sparsity of the file i.e. doesn't write zeroes
  // to the output file in locations corresponding to empty blocks in the input file.
  constexpr size_t kChunkSize = 64 * art::KB;

  std::vector<int8_t> data_buffer(/*count=*/kChunkSize, /*value=*/1);
  std::vector<int8_t> zero_buffer(/*count=*/kChunkSize, /*value=*/0);
  std::vector<int8_t> check_buffer(/*count=*/kChunkSize);

  auto verify = [&](size_t empty_prefix,
                    size_t empty_suffix,
                    size_t input_offset,
                    size_t outer_bytes,
                    size_t num_chunks) {
    constexpr size_t kEstimateMaxFileMetadataSize = 128 * art::KB;
    constexpr size_t kStatBlockSize = 512;
    art::ScratchFile src_tmp;
    FdFile src(src_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
    ASSERT_TRUE(src.IsOpened());

    /*
     * Layout of the source file:
     * - Skipped part:
     *    [ optional <input_offset> empty region ]
     *
     * - Copied part (skipping <outer_bytes> at start and end):
     *    [ optional <empty_prefix> empty region ]
     *    [ <kChunkSize> data chunk              ]  -\
     *    [ <kChunkSize> empty chunk             ]   |
     *    [ <kChunkSize> data chunk              ]   |
     *    [ <kChunkSize> empty chunk             ]    > (2 * num_chunks - 1) kChunkSize chunks
     *    [ <kChunkSize> data chunk              ]   |
     *    [   ...                                ]   |
     *    [ <kChunkSize> data chunk              ]  -/
     *    [ optional <empty_suffix> empty region ]
     *
     * To test the function is generic to copy an arbitrary length from an arbitrary offset within
     * the source file (where these might be in the middle of allocated data chunks), the start
     * range for copying is 'outer_bytes' into the copied part above, and the end range is
     * 'outer_bytes' from its end.
     */

    // For simplicity, avoid reducing the first and last data chunks by larger than the chunk size.
    ASSERT_LE(outer_bytes + empty_prefix, kChunkSize);
    ASSERT_LE(outer_bytes + empty_suffix, kChunkSize);

    int rc = lseek(src.Fd(), input_offset, SEEK_SET);
    ASSERT_EQ(rc, input_offset);

    rc = lseek(src.Fd(), empty_prefix, SEEK_CUR);
    ASSERT_EQ(rc, input_offset + empty_prefix);
    for (size_t i = 0; i < num_chunks; i++) {
      // Write data leaving chunk size of unwritten space in between them
      ASSERT_TRUE(src.WriteFully(data_buffer.data(), kChunkSize));
      rc = lseek(src.Fd(), kChunkSize, SEEK_CUR);
      ASSERT_NE(rc, -1);
    }

    ASSERT_EQ(0, src.SetLength(src.GetLength() + empty_suffix));
    ASSERT_EQ(0, src.Flush());

    size_t source_length = (2 * num_chunks - 1) * kChunkSize + empty_prefix + empty_suffix;
    ASSERT_EQ(static_cast<int64_t>(source_length), src.GetLength() - input_offset);

    struct stat src_stat;
    rc = fstat(src.Fd(), &src_stat);
    ASSERT_EQ(rc, 0);
    ASSERT_GE(src_stat.st_blocks, num_chunks * kChunkSize / kStatBlockSize);
    ASSERT_LE(src_stat.st_blocks,
              kEstimateMaxFileMetadataSize + num_chunks * kChunkSize / kStatBlockSize);

    art::ScratchFile dest_tmp;
    FdFile dest(dest_tmp.GetFilename(), O_RDWR, /*check_usage=*/false);
    ASSERT_TRUE(dest.IsOpened());

    ASSERT_TRUE(dest.Copy(&src,
                          input_offset + outer_bytes,
                          src.GetLength() - input_offset - (2 * outer_bytes)));
    ASSERT_EQ(0, dest.Flush());

    size_t dest_length = source_length - (2 * outer_bytes);
    ASSERT_EQ(static_cast<int64_t>(dest_length), dest.GetLength());

    // The dest file offset should be the end of the file.
    ASSERT_EQ(lseek(dest.Fd(), 0, SEEK_CUR), dest.GetLength());
    // The src file offset should be at the end of what we have copied from it.
    ASSERT_EQ(lseek(src.Fd(), 0, SEEK_CUR), src.GetLength() - outer_bytes);

    if (outer_bytes == 0) {
      // Number of blocks is only expected to be equal if the entire source file was copied, and it
      // was a sparsity-preserving copy.
      struct stat dest_stat;
      rc = fstat(dest.Fd(), &dest_stat);
      ASSERT_EQ(rc, 0);
      ASSERT_EQ(dest_stat.st_blocks, src_stat.st_blocks);
    }

    rc = lseek(dest.Fd(), 0, SEEK_SET);
    ASSERT_EQ(rc, 0);

    // Test the resulting data within the destination file is correct.

    // For testing partial copies, we skip an additional 'outer_bytes' of the start and end of the
    // source file. This has the effect of reducing the size of the regions we should expect in the
    // destination, calculated as follows.
    size_t first_hole_size = 0;
    size_t first_data_size = kChunkSize;
    if (outer_bytes > empty_prefix) {
      first_hole_size = 0;
      first_data_size -= (outer_bytes + empty_prefix);
    } else {
      first_hole_size = empty_prefix - outer_bytes;
    }
    size_t last_hole_size = 0;
    size_t last_data_size = kChunkSize;
    if (outer_bytes > empty_suffix) {
      last_hole_size = 0;
      last_data_size -= (outer_bytes - empty_suffix);
    } else {
      last_hole_size = empty_suffix - outer_bytes;
    }

    ASSERT_TRUE(dest.ReadFully(check_buffer.data(), first_hole_size));
    ASSERT_EQ(0, memcmp(check_buffer.data(), zero_buffer.data(), first_hole_size));

    ASSERT_TRUE(dest.ReadFully(check_buffer.data(), first_data_size));
    ASSERT_EQ(0, memcmp(check_buffer.data(), data_buffer.data(), first_data_size));

    for (size_t i = 1; i < 2 * num_chunks - 2; i++) {
      ASSERT_TRUE(dest.ReadFully(check_buffer.data(), kChunkSize));

      if (i % 2 == 0) {
        ASSERT_EQ(0, memcmp(check_buffer.data(), data_buffer.data(), kChunkSize));
      } else {
        ASSERT_EQ(0, memcmp(check_buffer.data(), zero_buffer.data(), kChunkSize));
      }
    }

    ASSERT_TRUE(dest.ReadFully(check_buffer.data(), last_data_size));
    ASSERT_EQ(0, memcmp(check_buffer.data(), data_buffer.data(), last_data_size));

    ASSERT_TRUE(dest.ReadFully(check_buffer.data(), last_hole_size));
    ASSERT_EQ(0, memcmp(check_buffer.data(), zero_buffer.data(), last_hole_size));

    ASSERT_EQ(0, dest.Close());
    ASSERT_EQ(0, src.Close());
  };

  auto full_subtest = [&](size_t empty_prefix, size_t empty_suffix) {
    // Copy full source file.
    verify(empty_prefix, empty_suffix, 0, 0, 16);
    verify(empty_prefix, empty_suffix, kChunkSize, 0, 16);
  };

  auto partial_subtest = [&](size_t discarded_outer_bytes) {
    // Test copying different offset and lengths relative to fixed source file.
    verify(0, 0, 0, discarded_outer_bytes, 16);
    verify(0, 0, kChunkSize, discarded_outer_bytes, 16);
  };

  // No empty prefix or suffix
  full_subtest(0, 0);

  for (size_t skip_size1 = 1; skip_size1 <= kChunkSize; skip_size1 <<= 2) {
    // Empty prefix only
    full_subtest(skip_size1, 0);
    // Empty suffix only
    full_subtest(0, skip_size1);
    // Empty prefix and suffix
    for (size_t skip_size2 = 1; skip_size2 <= kChunkSize; skip_size2 <<= 2) {
      full_subtest(skip_size1, skip_size2);
    }
    // Start and end the copy in the middle of the first/last data chunk
    partial_subtest(skip_size1);
  }

  // Test a larger file size with more chunks that should require more than one ioctl call.
  verify(0, 0, kChunkSize, 0, 512);
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
