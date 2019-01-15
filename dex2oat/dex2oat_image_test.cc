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

#include <future>
#include <regex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
// TODO: It might be good to pull in boringssl for better checksum functions (sha1).
#include <zlib.h>  // for adler32

#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/unique_fd.h>

#include "common_runtime_test.h"

#include "base/file_utils.h"
#include "base/macros.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex/method_reference.h"
#include "profile/profile_compilation_info.h"
#include "runtime.h"

namespace art {

static constexpr const char* kDefaultDex2OatSuffix = "";
static constexpr const char* kDex2Oat32Suffix = "32";
static constexpr const char* kDex2Oat64Suffix = "64";

struct ImageSizes {
  size_t art_size = 0;
  size_t oat_size = 0;
  size_t vdex_size = 0;
};

std::ostream& operator<<(std::ostream& os, const ImageSizes& sizes) {
  os << "art=" << sizes.art_size << " oat=" << sizes.oat_size << " vdex=" << sizes.vdex_size;
  return os;
}

class Dex2oatImageTest : public CommonRuntimeTest {
 public:
  void TearDown() override {}

 protected:
  // Visitors take method and type references
  template <typename MethodVisitor, typename ClassVisitor>
  void VisitLibcoreDexes(const MethodVisitor& method_visitor,
                         const ClassVisitor& class_visitor,
                         size_t method_frequency = 1,
                         size_t class_frequency = 1) {
    size_t method_counter = 0;
    size_t class_counter = 0;
    for (const std::string& dex : GetLibCoreDexFileNames()) {
      std::vector<std::unique_ptr<const DexFile>> dex_files;
      std::string error_msg;
      const ArtDexFileLoader dex_file_loader;
      CHECK(dex_file_loader.Open(dex.c_str(),
                                 dex,
                                 /*verify*/ true,
                                 /*verify_checksum*/ false,
                                 &error_msg,
                                 &dex_files))
          << error_msg;
      for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
        for (size_t i = 0; i < dex_file->NumMethodIds(); ++i) {
          if (++method_counter % method_frequency == 0) {
            method_visitor(MethodReference(dex_file.get(), i));
          }
        }
        for (size_t i = 0; i < dex_file->NumTypeIds(); ++i) {
          if (++class_counter % class_frequency == 0) {
            class_visitor(TypeReference(dex_file.get(), dex::TypeIndex(i)));
          }
        }
      }
    }
  }

  static void WriteLine(File* file, std::string line) {
    line += '\n';
    EXPECT_TRUE(file->WriteFully(&line[0], line.length()));
  }

  void GenerateClasses(File* out_file, size_t frequency = 1) {
    VisitLibcoreDexes(VoidFunctor(),
                      [out_file](TypeReference ref) {
      WriteLine(out_file, ref.dex_file->PrettyType(ref.TypeIndex()));
    }, frequency, frequency);
    EXPECT_EQ(out_file->Flush(), 0);
  }

  void GenerateMethods(File* out_file, size_t frequency = 1) {
    VisitLibcoreDexes([out_file](MethodReference ref) {
      WriteLine(out_file, ref.PrettyMethod());
    }, VoidFunctor(), frequency, frequency);
    EXPECT_EQ(out_file->Flush(), 0);
  }

  void AddRuntimeArg(std::vector<std::string>& args, const std::string& arg) {
    args.push_back("--runtime-arg");
    args.push_back(arg);
  }

  std::unordered_map<std::string, std::optional<uint32_t>> CompileImagesAndGetChecksums(
      ScratchFile& scratch,
      const std::string& dex2oat_suffix,
      const std::vector<std::string>& extra_args) {
    std::unordered_map<std::string, std::optional<uint32_t>> ret;
    size_t dirname_loc = scratch.GetFilename().rfind("/");
    CHECK_NE(dirname_loc, std::string::npos) << "No directory " << scratch.GetFilename();
    CHECK_LT(dirname_loc, scratch.GetFilename().size());
    std::string scratch_dir = scratch.GetFilename().substr(0, dirname_loc + 1);
    CHECK(!scratch_dir.empty()) << "No directory " << scratch.GetFilename();
    std::string scratch_name = scratch.GetFilename().substr(dirname_loc + 1, std::string::npos);
    CHECK(!scratch_name.empty()) << "No file " << scratch.GetFilename();
    std::string error_msg;
    if (!CompileBootImage(dex2oat_suffix, extra_args, scratch.GetFilename(), &error_msg)) {
      LOG(ERROR) << "Failed to compile image " << scratch.GetFilename() << error_msg;
    }
    auto cksum = [](DIR* dir, std::string file) -> std::optional<uint32_t> {
      android::base::unique_fd fdfile(openat(dirfd(dir), file.c_str(), O_RDONLY | O_CLOEXEC));
      std::string error;
      struct stat st;
      CHECK(fstat(fdfile, &st) == 0);
      MemMap map(MemMap::MapFile(st.st_size,
                                 PROT_READ,
                                 MAP_PRIVATE,
                                 fdfile,
                                 /* offset = */ 0u,
                                 /* low_4gb = */ false,
                                 file.c_str(),
                                 &error));
      if (!map.IsValid()) {
        return std::optional<uint32_t>{};
      }
      return std::optional {adler32(0u, map.Begin(), st.st_size)};
    };
    std::vector<std::pair<std::string, std::future<std::optional<uint32_t>>>> futures;
    DIR* dir;
    if ((dir = opendir(scratch_dir.c_str())) != nullptr) {
      struct dirent* entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type != DT_REG) {
          continue;
        }
        if (strncmp(entry->d_name, scratch_name.c_str(), scratch_name.length()) != 0) {
          // not a compilation artifact.
          continue;
        }
        std::string name(entry->d_name);
        std::string canonical_name =
            "<TEST>" + name.substr(scratch_name.length(), std::string::npos);
        futures.push_back(std::make_pair(canonical_name, std::async(cksum, dir, name)));
      }
    }

    for (auto& p : futures) {
      ret[p.first] = p.second.get();
    }

    // Clear image files since we compile the image multiple times and don't want to leave any
    // artifacts behind.
    closedir(dir);
    ClearDirectory(scratch_dir.c_str(), /*recursive*/ false);
    return ret;
  }

  ImageSizes CompileImageAndGetSizes(const std::vector<std::string>& extra_args) {
    ImageSizes ret;
    ScratchFile scratch;
    std::string scratch_dir = scratch.GetFilename();
    while (!scratch_dir.empty() && scratch_dir.back() != '/') {
      scratch_dir.pop_back();
    }
    CHECK(!scratch_dir.empty()) << "No directory " << scratch.GetFilename();
    std::string error_msg;
    if (!CompileBootImage(kDefaultDex2OatSuffix, extra_args, scratch.GetFilename(), &error_msg)) {
      LOG(ERROR) << "Failed to compile image " << scratch.GetFilename() << error_msg;
    }
    std::string art_file = scratch.GetFilename() + ".art";
    std::string oat_file = scratch.GetFilename() + ".oat";
    std::string vdex_file = scratch.GetFilename() + ".vdex";
    int64_t art_size = OS::GetFileSizeBytes(art_file.c_str());
    int64_t oat_size = OS::GetFileSizeBytes(oat_file.c_str());
    int64_t vdex_size = OS::GetFileSizeBytes(vdex_file.c_str());
    CHECK_GT(art_size, 0u) << art_file;
    CHECK_GT(oat_size, 0u) << oat_file;
    CHECK_GT(vdex_size, 0u) << vdex_file;
    ret.art_size = art_size;
    ret.oat_size = oat_size;
    ret.vdex_size = vdex_size;
    scratch.Close();
    // Clear image files since we compile the image multiple times and don't want to leave any
    // artifacts behind.
    ClearDirectory(scratch_dir.c_str(), /*recursive*/ false);
    return ret;
  }

  bool CompileBootImage(const std::string& dex2oat_suffix,
                        const std::vector<std::string>& extra_args,
                        const std::string& image_file_name_prefix,
                        std::string* error_msg) {
    Runtime* const runtime = Runtime::Current();
    std::vector<std::string> argv;
    argv.push_back(runtime->GetCompilerExecutable() + dex2oat_suffix);
    AddRuntimeArg(argv, "-Xms64m");
    AddRuntimeArg(argv, "-Xmx64m");
    std::vector<std::string> dex_files = GetLibCoreDexFileNames();
    for (const std::string& dex_file : dex_files) {
      argv.push_back("--dex-file=" + dex_file);
      argv.push_back("--dex-location=" + dex_file);
    }
    if (runtime->IsJavaDebuggable()) {
      argv.push_back("--debuggable");
    }
    runtime->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

    AddRuntimeArg(argv, "-Xverify:softfail");

    if (!kIsTargetBuild) {
      argv.push_back("--host");
    }

    argv.push_back("--image=" + image_file_name_prefix + ".art");
    argv.push_back("--oat-file=" + image_file_name_prefix + ".oat");
    argv.push_back("--oat-location=" + image_file_name_prefix + ".oat");
    argv.push_back("--base=0x60000000");

    std::vector<std::string> compiler_options = runtime->GetCompilerOptions();
    argv.insert(argv.end(), compiler_options.begin(), compiler_options.end());

    // We must set --android-root.
    const char* android_root = getenv("ANDROID_ROOT");
    CHECK(android_root != nullptr);
    argv.push_back("--android-root=" + std::string(android_root));
    argv.insert(argv.end(), extra_args.begin(), extra_args.end());

    return RunDex2Oat(argv, error_msg);
  }

  bool RunDex2Oat(const std::vector<std::string>& args, std::string* error_msg) {
    // We only want fatal logging for the error message.
    auto post_fork_fn = []() { return setenv("ANDROID_LOG_TAGS", "*:f", 1) == 0; };
    ForkAndExecResult res = ForkAndExec(args, post_fork_fn, error_msg);
    if (res.stage != ForkAndExecResult::kFinished) {
      *error_msg = strerror(errno);
      return false;
    }
    return res.StandardSuccess();
  }
};

TEST_F(Dex2oatImageTest, TestCompilerISA) {
  if (kIsTargetBuild) {
    // Test requires 32 & 64bit dex2oat binaries to run.
    return;
  }
  if (!kIsDebugBuild) {
    // 32 and 64 bit versions of dex2oat are only built for debug versions.
    return;
  }
  ScratchFile scratch;
  // Only want this for the directory.
  scratch.Unlink();
  std::unordered_map<std::string, std::optional<uint32_t>> res32 =
    CompileImagesAndGetChecksums(scratch, kDex2Oat32Suffix, {"--avoid-storing-invocation"});
  std::unordered_map<std::string, std::optional<uint32_t>> res64 =
    CompileImagesAndGetChecksums(scratch, kDex2Oat64Suffix, {"--avoid-storing-invocation"});
  scratch.Close();
  EXPECT_GE(res64.size(), 0u);
  EXPECT_EQ(res64.size(), res32.size());
  for (auto& a : res32) {
    auto other = res64.find(a.first);
    EXPECT_TRUE(other != res64.end()) << "Could not find " << a.first;
    EXPECT_EQ(a.second, other != res64.end() ? other->second : std::optional<uint32_t>())
         << "Different for file " << a.first;
  }
  // TODO
}

TEST_F(Dex2oatImageTest, TestModesAndFilters) {
  // This test crashes on the gtest-heap-poisoning configuration
  // (AddressSanitizer + CMS/RosAlloc + heap-poisoning); see b/111061592.
  // Temporarily disable this test on this configuration to keep
  // our automated build/testing green while we work on a fix.
  TEST_DISABLED_FOR_MEMORY_TOOL_WITH_HEAP_POISONING_WITHOUT_READ_BARRIERS();
  if (kIsTargetBuild) {
    // This test is too slow for target builds.
    return;
  }
  ImageSizes base_sizes = CompileImageAndGetSizes({});
  ImageSizes image_classes_sizes;
  ImageSizes compiled_classes_sizes;
  ImageSizes compiled_methods_sizes;
  ImageSizes profile_sizes;
  std::cout << "Base compile sizes " << base_sizes << std::endl;
  // Test image classes
  {
    ScratchFile classes;
    GenerateClasses(classes.GetFile(), /*frequency*/ 1u);
    image_classes_sizes = CompileImageAndGetSizes(
        {"--image-classes=" + classes.GetFilename()});
    classes.Close();
    std::cout << "Image classes sizes " << image_classes_sizes << std::endl;
    // Putting all classes as image classes should increase art size
    EXPECT_GE(image_classes_sizes.art_size, base_sizes.art_size);
    // Sanity check that dex is the same size.
    EXPECT_EQ(image_classes_sizes.vdex_size, base_sizes.vdex_size);
  }
  // Test compiled classes.
  {
    ScratchFile classes;
    // Only compile every even class.
    GenerateClasses(classes.GetFile(), /*frequency*/ 2u);
    compiled_classes_sizes = CompileImageAndGetSizes(
        {"--image-classes=" + classes.GetFilename()});
    classes.Close();
    std::cout << "Compiled classes sizes " << compiled_classes_sizes << std::endl;
    // Art file should be smaller than image classes version since we included fewer classes in the
    // list.
    EXPECT_LT(compiled_classes_sizes.art_size, image_classes_sizes.art_size);
  }
  static size_t kMethodFrequency = 3;
  static size_t kTypeFrequency = 4;
  // Test compiling fewer methods and classes.
  {
    ScratchFile classes;
    // Only compile every even class.
    GenerateClasses(classes.GetFile(), kTypeFrequency);
    compiled_methods_sizes = CompileImageAndGetSizes(
        {"--image-classes=" + classes.GetFilename()});
    classes.Close();
    std::cout << "Compiled fewer methods sizes " << compiled_methods_sizes << std::endl;
  }
  // Cross verify profile based image against image-classes and compiled-methods to make sure it
  // matches.
  {
    ProfileCompilationInfo profile;
    VisitLibcoreDexes([&profile](MethodReference ref) {
      uint32_t flags = ProfileCompilationInfo::MethodHotness::kFlagHot |
          ProfileCompilationInfo::MethodHotness::kFlagStartup;
      EXPECT_TRUE(profile.AddMethodIndex(
          static_cast<ProfileCompilationInfo::MethodHotness::Flag>(flags),
          ref));
    }, [&profile](TypeReference ref) {
      EXPECT_TRUE(profile.AddClassForDex(ref));
    }, kMethodFrequency, kTypeFrequency);
    ScratchFile profile_file;
    profile.Save(profile_file.GetFile()->Fd());
    EXPECT_EQ(profile_file.GetFile()->Flush(), 0);
    profile_sizes = CompileImageAndGetSizes(
        {"--profile-file=" + profile_file.GetFilename(),
         "--compiler-filter=speed-profile"});
    profile_file.Close();
    std::cout << "Profile sizes " << profile_sizes << std::endl;
    // Since there is some difference between profile vs image + methods due to layout, check that
    // the range is within expected margins (+-10%).
    const double kRatio = 0.90;
    EXPECT_LE(profile_sizes.art_size * kRatio, compiled_methods_sizes.art_size);
    // TODO(mathieuc): Find a reliable way to check compiled code. b/63746626
    // EXPECT_LE(profile_sizes.oat_size * kRatio, compiled_methods_sizes.oat_size);
    EXPECT_LE(profile_sizes.vdex_size * kRatio, compiled_methods_sizes.vdex_size);
    EXPECT_GE(profile_sizes.art_size / kRatio, compiled_methods_sizes.art_size);
    // TODO(mathieuc): Find a reliable way to check compiled code. b/63746626
    // EXPECT_GE(profile_sizes.oat_size / kRatio, compiled_methods_sizes.oat_size);
    EXPECT_GE(profile_sizes.vdex_size / kRatio, compiled_methods_sizes.vdex_size);
  }
  // Test dirty image objects.
  {
    ScratchFile classes;
    GenerateClasses(classes.GetFile(), /*frequency*/ 1u);
    image_classes_sizes = CompileImageAndGetSizes(
        {"--dirty-image-objects=" + classes.GetFilename()});
    classes.Close();
    std::cout << "Dirty image object sizes " << image_classes_sizes << std::endl;
  }
}

}  // namespace art
