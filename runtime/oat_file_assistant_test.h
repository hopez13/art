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

#ifndef ART_RUNTIME_OAT_FILE_ASSISTANT_TEST_H_
#define ART_RUNTIME_OAT_FILE_ASSISTANT_TEST_H_

#include <string>
#include <vector>

#include <backtrace/BacktraceMap.h>
#include <gtest/gtest.h>

#include "common_runtime_test.h"
#include "compiler_callbacks.h"
#include "dex2oat_environment_test.h"
#include "gc/space/image_space.h"
#include "mem_map.h"
#include "oat_file_assistant.h"

namespace art {

class OatFileAssistantTest : public Dex2oatEnvironmentTest {
 public:
  virtual void SetUp() OVERRIDE {
    ReserveImageSpace();
    Dex2oatEnvironmentTest::SetUp();
  }

  // Pre-Relocate the image to a known non-zero offset so we don't have to
  // deal with the runtime randomly relocating the image by 0 and messing up
  // the expected results of the tests.
  bool PreRelocateImage(const std::string& image_location, std::string* error_msg) {
    std::string image;
    if (!GetCachedImageFile(image_location, &image, error_msg)) {
      return false;
    }

    std::string patchoat = GetAndroidRoot();
    patchoat += kIsDebugBuild ? "/bin/patchoatd" : "/bin/patchoat";

    std::vector<std::string> argv;
    argv.push_back(patchoat);
    argv.push_back("--input-image-location=" + image_location);
    argv.push_back("--output-image-file=" + image);
    argv.push_back("--instruction-set=" + std::string(GetInstructionSetString(kRuntimeISA)));
    argv.push_back("--base-offset-delta=0x00008000");
    return Exec(argv, error_msg);
  }

  virtual void PreRuntimeCreate() {
    std::string error_msg;
    ASSERT_TRUE(PreRelocateImage(GetImageLocation(), &error_msg)) << error_msg;
    ASSERT_TRUE(PreRelocateImage(GetImageLocation2(), &error_msg)) << error_msg;
    UnreserveImageSpace();
  }

  virtual void PostRuntimeCreate() OVERRIDE {
    ReserveImageSpace();
  }

  // Generate an oat file for the purposes of test.
  void GenerateOatForTest(const std::string& dex_location,
                          const std::string& oat_location,
                          CompilerFilter::Filter filter,
                          bool relocate,
                          bool pic,
                          bool with_alternate_image) {
    std::string dalvik_cache = GetDalvikCache(GetInstructionSetString(kRuntimeISA));
    std::string dalvik_cache_tmp = dalvik_cache + ".redirected";

    if (!relocate) {
      // Temporarily redirect the dalvik cache so dex2oat doesn't find the
      // relocated image file.
      ASSERT_EQ(0, rename(dalvik_cache.c_str(), dalvik_cache_tmp.c_str())) << strerror(errno);
    }

    std::vector<std::string> args;
    args.push_back("--dex-file=" + dex_location);
    args.push_back("--oat-file=" + oat_location);
    args.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(filter));
    args.push_back("--runtime-arg");

    // Use -Xnorelocate regardless of the relocate argument.
    // We control relocation by redirecting the dalvik cache when needed
    // rather than use this flag.
    args.push_back("-Xnorelocate");

    if (pic) {
      args.push_back("--compile-pic");
    }

    std::string image_location = GetImageLocation();
    if (with_alternate_image) {
      args.push_back("--boot-image=" + GetImageLocation2());
    }

    std::string error_msg;
    ASSERT_TRUE(OatFileAssistant::Dex2Oat(args, &error_msg)) << error_msg;

    if (!relocate) {
      // Restore the dalvik cache if needed.
      ASSERT_EQ(0, rename(dalvik_cache_tmp.c_str(), dalvik_cache.c_str())) << strerror(errno);
    }

    // Verify the odex file was generated as expected.
    std::unique_ptr<OatFile> odex_file(OatFile::Open(oat_location.c_str(),
                                                     oat_location.c_str(),
                                                     nullptr,
                                                     nullptr,
                                                     false,
                                                     /*low_4gb*/false,
                                                     dex_location.c_str(),
                                                     &error_msg));
    ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;
    EXPECT_EQ(pic, odex_file->IsPic());
    EXPECT_EQ(filter, odex_file->GetCompilerFilter());

    std::unique_ptr<ImageHeader> image_header(
            gc::space::ImageSpace::ReadImageHeader(image_location.c_str(),
                                                   kRuntimeISA,
                                                   &error_msg));
    ASSERT_TRUE(image_header != nullptr) << error_msg;
    const OatHeader& oat_header = odex_file->GetOatHeader();
    uint32_t combined_checksum = OatFileAssistant::CalculateCombinedImageChecksum();

    if (CompilerFilter::DependsOnImageChecksum(filter)) {
      if (with_alternate_image) {
        EXPECT_NE(combined_checksum, oat_header.GetImageFileLocationOatChecksum());
      } else {
        EXPECT_EQ(combined_checksum, oat_header.GetImageFileLocationOatChecksum());
      }
    }

    if (CompilerFilter::IsBytecodeCompilationEnabled(filter)) {
      if (relocate) {
        EXPECT_EQ(reinterpret_cast<uintptr_t>(image_header->GetOatDataBegin()),
            oat_header.GetImageFileLocationOatDataBegin());
        EXPECT_EQ(image_header->GetPatchDelta(), oat_header.GetImagePatchDelta());
      } else {
        EXPECT_NE(reinterpret_cast<uintptr_t>(image_header->GetOatDataBegin()),
            oat_header.GetImageFileLocationOatDataBegin());
        EXPECT_NE(image_header->GetPatchDelta(), oat_header.GetImagePatchDelta());
      }
    }
  }

  // Generate a non-PIC odex file for the purposes of test.
  // The generated odex file will be un-relocated.
  void GenerateOdexForTest(const std::string& dex_location,
                           const std::string& odex_location,
                           CompilerFilter::Filter filter) {
    GenerateOatForTest(dex_location,
                       odex_location,
                       filter,
                       /*relocate*/false,
                       /*pic*/false,
                       /*with_alternate_image*/false);
  }

  void GeneratePicOdexForTest(const std::string& dex_location,
                              const std::string& odex_location,
                              CompilerFilter::Filter filter) {
    GenerateOatForTest(dex_location,
                       odex_location,
                       filter,
                       /*relocate*/false,
                       /*pic*/true,
                       /*with_alternate_image*/false);
  }

  // Generate an oat file in the oat location.
  void GenerateOatForTest(const char* dex_location,
                          CompilerFilter::Filter filter,
                          bool relocate,
                          bool pic,
                          bool with_alternate_image) {
    std::string oat_location;
    std::string error_msg;
    ASSERT_TRUE(OatFileAssistant::DexLocationToOatFilename(
          dex_location, kRuntimeISA, &oat_location, &error_msg)) << error_msg;
    GenerateOatForTest(dex_location,
                       oat_location,
                       filter,
                       relocate,
                       pic,
                       with_alternate_image);
  }

  // Generate a standard oat file in the oat location.
  void GenerateOatForTest(const char* dex_location, CompilerFilter::Filter filter) {
    GenerateOatForTest(dex_location,
                       filter,
                       /*relocate*/true,
                       /*pic*/false,
                       /*with_alternate_image*/false);
  }

 private:
  // Reserve memory around where the image will be loaded so other memory
  // won't conflict when it comes time to load the image.
  // This can be called with an already loaded image to reserve the space
  // around it.
  void ReserveImageSpace() {
    MemMap::Init();

    // Ensure a chunk of memory is reserved for the image space.
    // The reservation_end includes room for the main space that has to come
    // right after the image in case of the GSS collector.
    uintptr_t reservation_start = ART_BASE_ADDRESS;
    uintptr_t reservation_end = ART_BASE_ADDRESS + 384 * MB;

    std::unique_ptr<BacktraceMap> map(BacktraceMap::Create(getpid(), true));
    ASSERT_TRUE(map.get() != nullptr) << "Failed to build process map";
    for (BacktraceMap::const_iterator it = map->begin();
        reservation_start < reservation_end && it != map->end(); ++it) {
      ReserveImageSpaceChunk(reservation_start, std::min(it->start, reservation_end));
      reservation_start = std::max(reservation_start, it->end);
    }
    ReserveImageSpaceChunk(reservation_start, reservation_end);
  }

  // Reserve a chunk of memory for the image space in the given range.
  // Only has effect for chunks with a positive number of bytes.
  void ReserveImageSpaceChunk(uintptr_t start, uintptr_t end) {
    if (start < end) {
      std::string error_msg;
      image_reservation_.push_back(std::unique_ptr<MemMap>(
          MemMap::MapAnonymous("image reservation",
              reinterpret_cast<uint8_t*>(start), end - start,
              PROT_NONE, false, false, &error_msg)));
      ASSERT_TRUE(image_reservation_.back().get() != nullptr) << error_msg;
      LOG(INFO) << "Reserved space for image " <<
        reinterpret_cast<void*>(image_reservation_.back()->Begin()) << "-" <<
        reinterpret_cast<void*>(image_reservation_.back()->End());
    }
  }

  // Unreserve any memory reserved by ReserveImageSpace. This should be called
  // before the image is loaded.
  void UnreserveImageSpace() {
    image_reservation_.clear();
  }

  std::vector<std::unique_ptr<MemMap>> image_reservation_;
};

class OatFileAssistantNoDex2OatTest : public OatFileAssistantTest {
 public:
  virtual void SetUpRuntimeOptions(RuntimeOptions* options) {
    OatFileAssistantTest::SetUpRuntimeOptions(options);
    options->push_back(std::make_pair("-Xnodex2oat", nullptr));
  }
};

}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_ASSISTANT_TEST_H_
