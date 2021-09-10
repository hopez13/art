/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "libdexopt.h"
#define LOG_TAG "libdexopt"

#include <string>
#include <vector>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "android-base/result.h"
#include "base/file_utils.h"
#include "log/log.h"

#include "aidl/com/android/art/CompilerFilter.h"
#include "aidl/com/android/art/DexoptBcpExtArgs.h"
#include "aidl/com/android/art/DexoptSystemServerArgs.h"
#include "aidl/com/android/art/Isa.h"

namespace {

using aidl::com::android::art::CompilerFilter;
using aidl::com::android::art::DexoptBcpExtArgs;
using aidl::com::android::art::DexoptSystemServerArgs;
using aidl::com::android::art::Isa;
using android::base::Error;
using android::base::Result;

std::string GetBootImage() {
  // Typically "/apex/com.android.art/javalib/boot.art".
  return art::GetArtRoot() + "/javalib/boot.art";
}

std::string GetEnvironmentVariableOrDie(const char* name) {
  const char* value = getenv(name);
  LOG_ALWAYS_FATAL_IF(value == nullptr, "%s is not defined.", name);
  return value;
}

std::string GetDex2oatBootClasspath() {
  return GetEnvironmentVariableOrDie("DEX2OATBOOTCLASSPATH");
}

std::string GetBootClasspath() {
  return GetEnvironmentVariableOrDie("BOOTCLASSPATH");
}

std::string ToInstructionSetString(Isa isa) {
  switch (isa) {
    case Isa::ARM:
    case Isa::THUMB2:
      return "arm";
    case Isa::ARM64:
      return "arm64";
    case Isa::X86:
      return "x86";
    case Isa::X86_64:
      return "x86_64";
    default:
      UNREACHABLE();
  }
}

const char* CompilerFilterAidlToString(CompilerFilter compiler_filter) {
  switch (compiler_filter) {
    case CompilerFilter::SPEED_PROFILE:
      return "speed-profile";
    case CompilerFilter::SPEED:
      return"speed";
    case CompilerFilter::VERIFY:
      return "verify";
    default:
      UNREACHABLE();
  }
}

Result<void> AddBootClasspathFds(/*inout*/ std::vector<std::string>& cmdline,
                                 const std::vector<int32_t>& boot_classpath_fds) {
  if (boot_classpath_fds.empty()) {
    return Errorf("Missing BCP file descriptors");
  }
  cmdline.emplace_back("--runtime-arg");
  cmdline.emplace_back("-Xbootclasspathfds:" + android::base::Join(boot_classpath_fds, ':'));
  return {};
}

void AddCompiledBootClasspathFdsIfAny(/*inout*/ std::vector<std::string>& cmdline,
                                      const DexoptSystemServerArgs& args) {
  DCHECK(args.bootClasspathImageFds.size() == args.bootClasspathOatFds.size());
  DCHECK(args.bootClasspathImageFds.size() == args.bootClasspathVdexFds.size());

  if (!args.bootClasspathImageFds.empty()) {
    cmdline.emplace_back("--runtime-arg");
    cmdline.emplace_back(
        "-Xbootclasspathimagefds:" + android::base::Join(args.bootClasspathImageFds, ':'));
    cmdline.emplace_back("--runtime-arg");
    cmdline.emplace_back(
        "-Xbootclasspathoatfds:" + android::base::Join(args.bootClasspathOatFds, ':'));
    cmdline.emplace_back("--runtime-arg");
    cmdline.emplace_back(
        "-Xbootclasspathvdexfds:" + android::base::Join(args.bootClasspathVdexFds, ':'));
  }
}

void AddDex2OatConcurrencyArguments(/*inout*/ std::vector<std::string>& cmdline,
                                    int threads,
                                    const std::vector<int>& cpu_set) {
  if (threads > 0) {
      cmdline.emplace_back(android::base::StringPrintf("-j%d", threads));
  }
  if (!cpu_set.empty()) {
      cmdline.emplace_back("--cpu-set=" + android::base::Join(cpu_set, ':'));
  }
}

void AddDex2OatCommonOptions(/*inout*/ std::vector<std::string>& cmdline) {
  cmdline.emplace_back("--android-root=out/empty");
  cmdline.emplace_back("--abort-on-hard-verifier-error");
  cmdline.emplace_back("--no-abort-on-soft-verifier-error");
  cmdline.emplace_back("--compilation-reason=boot");
  cmdline.emplace_back("--image-format=lz4");
  cmdline.emplace_back("--force-determinism");
  cmdline.emplace_back("--resolve-startup-const-strings=true");
}

void AddDex2OatDebugInfo(/*inout*/ std::vector<std::string>& cmdline) {
  cmdline.emplace_back("--generate-mini-debug-info");
  cmdline.emplace_back("--strip");
}

}  // namespace

namespace art {

Result<void> AddDex2oatArgsFromBcpExtensionArgs(const DexoptBcpExtArgs& args,
                                                /*out*/ std::vector<std::string>& cmdline) {
  // Common dex2oat flags
  AddDex2OatCommonOptions(cmdline);
  AddDex2OatDebugInfo(cmdline);

  cmdline.emplace_back("--instruction-set=" + ToInstructionSetString(args.isa));

  if (args.profileFd >= 0) {
    cmdline.emplace_back(android::base::StringPrintf("--profile-file-fd=%d", args.profileFd));
    cmdline.emplace_back("--compiler-filter=speed-profile");
  } else {
    cmdline.emplace_back("--compiler-filter=speed");
  }

  // Compile as a single image for fewer files and slightly less memory overhead.
  cmdline.emplace_back("--single-image");

  // Set boot-image and expectation of compiling boot classpath extensions.
  cmdline.emplace_back("--boot-image=" + GetBootImage());

  if (args.dirtyImageObjectsFd >= 0) {
    cmdline.emplace_back(android::base::StringPrintf("--dirty-image-objects-fd=%d",
                                                     args.dirtyImageObjectsFd));
  }

  if (args.dexPaths.size() != args.dexFds.size()) {
    return Errorf("Mismatched number of dexPaths (%d) and dexFds (%d)",
                  args.dexPaths.size(),
                  args.dexFds.size());
  }
  for (unsigned int i = 0; i < args.dexPaths.size(); ++i) {
    cmdline.emplace_back("--dex-file=" + args.dexPaths[i]);
    cmdline.emplace_back(android::base::StringPrintf("--dex-fd=%d", args.dexFds[i]));
  }

  // BCP needs to be constructed in the execution environment.
  std::string dex2oat_bcp = GetDex2oatBootClasspath();
  cmdline.emplace_back("--runtime-arg");
  cmdline.emplace_back("-Xbootclasspath:" + dex2oat_bcp);
  auto bcp_jars = android::base::Split(dex2oat_bcp, ":");
  auto result = AddBootClasspathFds(cmdline, args.bootClasspathFds);
  if (!result.ok()) {
    return result.error();
  }

  cmdline.emplace_back("--oat-location=" + args.oatLocation);

  // Output files
  if (args.imageFd < 0) {
    return Error() << "imageFd is missing";
  }
  cmdline.emplace_back(android::base::StringPrintf("--image-fd=%d", args.imageFd));
  if (args.oatFd < 0) {
    return Error() << "oatFd is missing";
  }
  cmdline.emplace_back(android::base::StringPrintf("--oat-fd=%d", args.oatFd));
  if (args.vdexFd < 0) {
    return Error() << "vdexFd is missing";
  }
  cmdline.emplace_back(android::base::StringPrintf("--output-vdex-fd=%d", args.vdexFd));

  AddDex2OatConcurrencyArguments(cmdline, args.threads, args.cpuSet);

  return {};
}

Result<void> AddDex2oatArgsFromSystemServerArgs(const DexoptSystemServerArgs& args,
                                                /*out*/ std::vector<std::string>& cmdline) {
  cmdline.emplace_back("--dex-file=" + args.dexPath);
  cmdline.emplace_back(android::base::StringPrintf("--dex-fd=%d", args.dexFd));

  // Common dex2oat flags
  AddDex2OatCommonOptions(cmdline);
  AddDex2OatDebugInfo(cmdline);

  cmdline.emplace_back("--instruction-set=" + ToInstructionSetString(args.isa));

  if (args.compilerFilter == CompilerFilter::SPEED_PROFILE && args.profileFd >= 0) {
    cmdline.emplace_back(android::base::StringPrintf("--profile-file-fd=%d", args.profileFd));
    cmdline.emplace_back("--compiler-filter=speed-profile");
  } else {
    cmdline.emplace_back("--compiler-filter=" +
                         std::string(CompilerFilterAidlToString(args.compilerFilter)));
  }

  cmdline.emplace_back(android::base::StringPrintf("--app-image-fd=%d", args.imageFd));
  cmdline.emplace_back(android::base::StringPrintf("--oat-fd=%d", args.oatFd));
  cmdline.emplace_back(android::base::StringPrintf("--output-vdex-fd=%d", args.vdexFd));
  cmdline.emplace_back("--oat-location=" + args.oatLocation);

  if (args.updatableBcpPackagesTxtFd >= 0) {
    cmdline.emplace_back(android::base::StringPrintf("--updatable-bcp-packages-fd=%d",
                                                     args.updatableBcpPackagesTxtFd));
  }

  cmdline.emplace_back("--runtime-arg");
  cmdline.emplace_back("-Xbootclasspath:" + GetBootClasspath());
  auto result = AddBootClasspathFds(cmdline, args.bootClasspathFds);
  if (!result.ok()) {
    return result.error();
  }
  AddCompiledBootClasspathFdsIfAny(cmdline, args);

  if (args.classloaderFds.empty()) {
    cmdline.emplace_back("--class-loader-context=PCL[]");
  } else {
    const std::string context_path = android::base::Join(args.classloaderContext, ':');
    cmdline.emplace_back("--class-loader-context=PCL[" + context_path + "]");
    cmdline.emplace_back("--class-loader-context-fds=" +
                         android::base::Join(args.classloaderFds, ':'));
  }

  // Derive boot image
  // b/197176583
  std::vector<std::string> jar_paths = android::base::Split(GetDex2oatBootClasspath(), ":");
  auto iter = std::find_if_not(jar_paths.begin(), jar_paths.end(), &LocationIsOnArtModule);
  if (iter == jar_paths.end()) {
    return Error() << "Missing BCP extension compatible JAR";
  }
  const std::string& first_boot_extension_compatible_jars = *iter;
  // TODO(197176583): Support compiling against BCP extension in /system.
  const std::string extension_image = GetApexDataBootImage(first_boot_extension_compatible_jars);
  if (extension_image.empty()) {
    return Error() << "Can't identify the first boot extension compatible jar";
  }
  cmdline.emplace_back("--boot-image=" + GetBootImage() + ":" + extension_image);

  AddDex2OatConcurrencyArguments(cmdline, args.threads, args.cpuSet);

  return {};
}

}  // namespace art
