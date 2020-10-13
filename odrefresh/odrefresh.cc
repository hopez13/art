/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "odrefresh/odrefresh.h"

#include <dirent.h>
#include <ftw.h>
#include <sysexits.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <cstdarg>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <sstream>
#include <vector>

#include <tinyxml2.h>

#include <android/log.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <arch/instruction_set.h>
#include <base/bit_utils.h>
#include <base/file_utils.h>
#include <base/globals.h>
#include <base/macros.h>
#include <base/os.h>
#include <base/string_view_cpp20.h>
#include <base/unix_file/fd_file.h>
#include <base/utils.h>
#include <exec_utils.h>
#include <palette/palette.h>
#include <palette/palette_types.h>

#include "../dexoptanalyzer/dexoptanalyzer.h"

namespace art {
namespace odrefresh {
namespace {

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  if (isatty(fileno(stderr))) {
    std::cerr << error << std::endl;
  } else {
    LOG(ERROR) << error;
  }
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void ArgumentError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
  UsageError("Try '--help' for more information.");
  exit(EX_USAGE);
}

NO_RETURN static void UsageHelp(const char* argv0) {
  std::string name(Basename(argv0));
  UsageError("Usage: %s ACTION", name.c_str());
  UsageError("On-device refresh tool for boot class path extensions and system server");
  UsageError("following an update of the ART APEX.");
  UsageError("");
  UsageError("Valid ACTION choices are:");
  UsageError("");
  UsageError("--check          Check compilation artifacts are up to date.");
  UsageError("--compile        Compile boot class path extensions and system_server jars");
  UsageError("                 when necessary).");
  UsageError("--force-compile  Unconditionally compile the boot class path extensions and");
  UsageError("                 system_server jars.");
  UsageError("--help           Display this help information.");
  exit(EX_USAGE);
}

static std::string Concatenate(std::initializer_list<std::string_view> args) {
  std::stringstream ss;
  for (auto arg : args) {
    ss << arg;
  }
  return ss.str();
}

static std::string QuotePath(std::string_view path) {
  return Concatenate({"'", path, "'"});
}

static void EraseFiles(std::vector<std::unique_ptr<File>>& files) {
  for (auto& file : files) {
    file->Erase(/*unlink=*/ true);
  }
}

static bool MoveOrEraseFiles(std::vector<std::unique_ptr<File>>& files,
                             std::string_view output_directory_path) {
  for (auto& file : files) {
    const std::string file_basename(Basename(file->GetPath()));
    const std::string output_file_path = Concatenate({output_directory_path, "/", file_basename});
    const std::string input_file_path = file->GetPath();

    std::unique_ptr<File> output_file(OS::CreateEmptyFileWriteOnly(output_file_path.c_str()));
    if (output_file == nullptr) {
      PLOG(ERROR) << "Failed to open " << QuotePath(output_file_path);
      EraseFiles(files);
      output_file->Erase();
      return false;
    }

    const size_t file_bytes = file->GetLength();
    if (!output_file->Copy(file.get(), /*offset=*/ 0, file_bytes)) {
      PLOG(ERROR) << "Failed to copy " << QuotePath(file->GetPath()) << " to " << QuotePath(output_file_path);
      EraseFiles(files);
      output_file->Erase();
      return false;
    }

    if (!file->Erase(/*unlink=*/ true)) {
      PLOG(ERROR) << "Failed to erase " << QuotePath(file->GetPath());
      EraseFiles(files);
      output_file->Erase();
      return false;
    }

    if (output_file->FlushCloseOrErase() != 0) {
      PLOG(ERROR) << "Failed to flush and close file " << QuotePath(output_file_path);
      EraseFiles(files);
    }
  }
  return true;
}

}  // namespace

enum class ZygoteKind : uint8_t {
  kZygote32 = 0,
  kZygote32_64 = 1,
  kZygote64_32 = 2,
  kZygote64 = 3
};

bool ParseZygoteKind(const char* input, ZygoteKind* zygote_kind) {
  std::string_view z(input);
  if (z == "zygote32") {
    *zygote_kind = ZygoteKind::kZygote32;
    return true;
  } else if (z == "zygote32_64") {
    *zygote_kind = ZygoteKind::kZygote32_64;
    return true;
  } else if (z == "zygote64_32") {
    *zygote_kind = ZygoteKind::kZygote64_32;
    return true;
  } else if (z == "zygote64") {
    *zygote_kind = ZygoteKind::kZygote64;
    return true;
  }
  return false;
}

class OdrConfig final {
 public:
  OdrConfig() : dry_run_(false), isa_(InstructionSet::kNone) {}

  const std::string& GetAndroidRoot() const { return android_root_; }

  const std::string& GetApexInfoListFile() const { return apex_info_list_file_; }

  std::vector<InstructionSet> GetBootExtensionIsas() const {
    const auto [isa32, isa64] = GetInstructionSets();
    switch (zygote_kind_) {
      case ZygoteKind::kZygote32: return { isa32 };
      case ZygoteKind::kZygote32_64:
      case ZygoteKind::kZygote64_32: return { isa32, isa64 };
      case ZygoteKind::kZygote64: return { isa64 };
    }
  }

  InstructionSet GetSystemServerIsa() const {
    const auto [isa32, isa64] = GetInstructionSets();
    switch (zygote_kind_) {
      case ZygoteKind::kZygote32: return isa32;
      case ZygoteKind::kZygote32_64: return isa32;
      case ZygoteKind::kZygote64_32: return isa64;
      case ZygoteKind::kZygote64: return isa64;
    }
  }

  const std::vector<std::string>& GetDex2oatBootclasspath() const { return dex2oat_bootclasspath_; }

  std::string GetDex2Oat() const {
    switch (zygote_kind_) {
      case ZygoteKind::kZygote32:
        return art_bin_dir_ + "/dex2oat32";
      case ZygoteKind::kZygote32_64:
      case ZygoteKind::kZygote64_32:
      case ZygoteKind::kZygote64:
        return art_bin_dir_ + "/dex2oat64";
    }
  }

  std::string GetDexOptAnalyzer() const {
    return art_bin_dir_ + "/dexoptanalyzer";
  }

  bool GetDryRun() const { return dry_run_; }

  const std::vector<std::string>& GetSystemServerClasspath() const {
    return system_server_classpath_;
  }

  const std::string& GetUpdatableBcpPackagesFile() const { return updatable_bcp_packages_file_; }

  void SetAndroidRoot(const std::string& root) { android_root_ = root; }

  void SetApexInfoListFile(const std::string& file_path) { apex_info_list_file_ = file_path; }

  void SetArtBinDir(const std::string& art_bin_dir) { art_bin_dir_ = art_bin_dir; }

  void SetDex2oatBootclasspath(const std::string& v) {
    dex2oat_bootclasspath_ = android::base::Split(v, ":");
  }

  void SetDryRun() { dry_run_ = true; }
  void SetIsa(const InstructionSet isa) { isa_ = isa; }

  void SetSystemServerClasspath(const std::string& v) {
    system_server_classpath_ = android::base::Split(v, ":");
  }

  void SetUpdatableBcpPackagesFile(const std::string& file) { updatable_bcp_packages_file_ = file; }
  void SetZygoteKind(const ZygoteKind zygote_kind) { zygote_kind_ = zygote_kind; }

 private:
  std::pair<InstructionSet, InstructionSet> GetInstructionSets() const {
    switch (isa_) {
      case art::InstructionSet::kArm:
      case art::InstructionSet::kArm64:
        return std::make_pair(art::InstructionSet::kArm, art::InstructionSet::kArm64);
      case art::InstructionSet::kX86:
      case art::InstructionSet::kX86_64:
        return std::make_pair(art::InstructionSet::kX86, art::InstructionSet::kX86_64);
      case art::InstructionSet::kThumb2:
      case art::InstructionSet::kNone:
        LOG(FATAL) << "Invalid instruction set " << isa_;
        return std::make_pair(art::InstructionSet::kNone, art::InstructionSet::kNone);
    }
  }

  std::string android_root_;
  std::string apex_info_list_file_;
  std::string art_bin_dir_;
  std::vector<std::string> dex2oat_bootclasspath_;
  bool dry_run_;
  InstructionSet isa_;
  std::vector<std::string> system_server_classpath_;
  std::string updatable_bcp_packages_file_;
  ZygoteKind zygote_kind_;

  DISALLOW_COPY_AND_ASSIGN(OdrConfig);
};

class OnDeviceRefresh final {
 private:
  const OdrConfig& config_;

  std::string boot_extension_output_dir_;
  std::vector<std::string> boot_extension_compilable_jars_;

  std::string systemserver_output_dir_;
  std::vector<std::string> systemserver_compilable_jars_;

 public:
  explicit OnDeviceRefresh(const OdrConfig& config) : config_(config) {
    std::string art_apex_data = GetArtApexData();
    boot_extension_output_dir_ = Concatenate({art_apex_data, "/system/framework"});
    for (const auto& component : config_.GetDex2oatBootclasspath()) {
      if (IsCompilableBootExtension(component)) {
        boot_extension_compilable_jars_.emplace_back(component);
      }
    }

    systemserver_output_dir_ = Concatenate({art_apex_data, "/system/framework/oat"});
    for (const auto& component : config_.GetSystemServerClasspath()) {
      if (IsCompilableSystemServerJar(component)) {
        systemserver_compilable_jars_.emplace_back(component);
      }
    }
  }

  // Read apex_info_list.xml from input stream and determine if the ART APEX
  // listed is the factory installed version.
  bool IsFactoryApex(const char* apex_info_list_xml_path) const {
    tinyxml2::XMLDocument doc;
    doc.LoadFile(apex_info_list_xml_path);
    tinyxml2::XMLElement* e = doc.FirstChildElement("apex-info");
    while (e != nullptr) {
      const char* module_name = nullptr;
      if (e->QueryStringAttribute("moduleName", &module_name) != tinyxml2::XML_SUCCESS) {
        LOG(FATAL) << "moduleName not found";
      }
      if (strcmp("com.android.art", module_name) != tinyxml2::XML_SUCCESS) {
        e = e->NextSiblingElement();
        continue;
      }
      bool is_active;
      if (e->QueryBoolAttribute("isActive", &is_active)) {
        LOG(FATAL) << "missing attribute isActive";
      }
      bool is_factory;
      if (e->QueryBoolAttribute("isFactory", &is_factory) != tinyxml2::XML_SUCCESS) {
        LOG(FATAL) << "missing attribute isFound";
      }
      return is_factory;
    }
    return false;
  }

  static void AddDex2OatBootTimeArguments(std::vector<std::string>* args) {
    static constexpr std::pair<const char*, const char*> kPropertyArgPairs[] = {
      std::make_pair("dalvik.vm.boot-dex2oat-cpu-set", "--cpu-set="),
      std::make_pair("dalvik.vm.boot-dex2oat-threads", "-j"),
    };
    for (auto property_arg_pair : kPropertyArgPairs) {
      auto [property, arg] = property_arg_pair;
      std::string value = android::base::GetProperty(property, {});
      if (!value.empty()) {
        args->push_back(arg + value);
      }
    }
  }

  bool CheckSystemServerJars() const {
    int missing_files = 0;
    for (const std::string& jar_path : systemserver_compilable_jars_) {
      const std::string app_image_path = GetSystemServerAppImageLocation(jar_path);
      for (auto extension : {"art", "odex", "vdex"}) {
        std::string path = ReplaceFileExtension(app_image_path, extension);
        if (!OS::FileExists(path.c_str(), true)) {
          LOG(INFO) << "Missing " << path;
          missing_files++;
        }
      }
    }

    if (missing_files != 0) {
      return false;
    }

    std::vector<std::string> classloader_context;
    for (const std::string& jar_path : systemserver_compilable_jars_) {
      std::vector<std::string> args;
      args.emplace_back(config_.GetDexOptAnalyzer());
      args.emplace_back("--dex-file=" + jar_path);

      const std::string app_image_path = GetSystemServerAppImageLocation(jar_path);

      // Open file descriptors for dexoptanalyzer file inputs and add to the command-line.
      const std::string paths[3] = {
        ReplaceFileExtension(app_image_path, "odex"),
        ReplaceFileExtension(app_image_path, "vdex"),
        jar_path
      };
      constexpr const char* kFdArg[std::size(paths)] = {"--oat-fd=", "--vdex-fd=", "--zip-fd="};
      std::unique_ptr<File> files[std::size(paths)];
      for (size_t i = 0; i < std::size(paths); ++i) {
        files[i].reset(OS::OpenFileForReading(paths[i].c_str()));
        if (files[i] == nullptr) {
          PLOG(ERROR) << "Failed to open \"" << paths[i] << "\"";
          return false;
        }
        args.emplace_back(android::base::StringPrintf("%s%d", kFdArg[i], files[i]->Fd()));
      }

      const std::string basename(Basename(jar_path));
      std::string profile_file = Concatenate({GetAndroidRoot(), "/framework/", basename, ".prof"});
      if (OS::FileExists(profile_file.c_str())) {
        args.emplace_back("--compiler-filter=speed-profile");
      } else {
        args.emplace_back("--compiler-filter=speed");
      }

      // TBD: app_image_path is not valid in the image list.
      args.emplace_back(
        Concatenate({"--image=", GetBootImage(), ":", GetBootImageExtensionImage()}));
      args.emplace_back(
        Concatenate({"--isa=", GetInstructionSetString(config_.GetSystemServerIsa())}));
      args.emplace_back("--runtime-arg");
      args.emplace_back(
        "-Xbootclasspath:" + android::base::Join(config_.GetDex2oatBootclasspath(), ':'));
      args.emplace_back(Concatenate({
        "--class-loader-context=PCL[", android::base::Join(classloader_context, ':'), "]"}));

      classloader_context.emplace_back(jar_path);

      LOG(INFO) << "Checking " << jar_path << ": " << android::base::Join(args, ' ');
      if (config_.GetDryRun()) {
        return true;
      }

      std::string error_msg;
      const int dexoptanalyzer_result = ExecAndReturnCode(args, &error_msg);
      switch (static_cast<dexoptanalyzer::ReturnCode>(dexoptanalyzer_result)) {
        case art::dexoptanalyzer::ReturnCode::kNoDexOptNeeded:
          break;

        // Recompile needed
        case art::dexoptanalyzer::ReturnCode::kDex2OatFromScratch:
        case art::dexoptanalyzer::ReturnCode::kDex2OatForBootImageOat:
        case art::dexoptanalyzer::ReturnCode::kDex2OatForFilterOat:
        case art::dexoptanalyzer::ReturnCode::kDex2OatForBootImageOdex:
        case art::dexoptanalyzer::ReturnCode::kDex2OatForFilterOdex:
          return false;

        // Unexpected issues
        case art::dexoptanalyzer::ReturnCode::kFlattenClassLoaderContextSuccess:
        case art::dexoptanalyzer::ReturnCode::kErrorInvalidArguments:
        case art::dexoptanalyzer::ReturnCode::kErrorCannotCreateRuntime:
        case art::dexoptanalyzer::ReturnCode::kErrorUnknownDexOptNeeded:

          LOG(ERROR) << "Unexpected result from dexoptanalyzer (" << dexoptanalyzer_result;
          return false;
      }
    }

    return true;
  }

  bool CheckBootExtensionsOnSystem() const {
    // odrefresh compiles boot extensions as single image so there is only one output file of each
    // type (.art, .oat, .vdex).
    for (const InstructionSet isa : config_.GetBootExtensionIsas()) {
      for (const std::string& component : boot_extension_compilable_jars_) {
        std::string name(Basename(component, ".jar"));
        for (const auto& extension : {".art", ".oat", ".vdex"}) {
          const char* arch = GetInstructionSetString(isa);
          std::string path =
            Concatenate({GetAndroidRoot(), "/framework/", arch, "/boot-", name, extension});
          if (!OS::FileExists(path.c_str(), true)) {
            LOG(INFO) << "Missing " << path;
            return false;
          }
        }
      }
    }
    return true;
  }

  bool CheckSystemServerOnSystem() const {
    for (const std::string& component : systemserver_compilable_jars_) {
      std::string name(Basename(component, ".jar"));
      for (const auto& extension : {".odex", ".vdex"}) {
        const char* arch = GetInstructionSetString(config_.GetSystemServerIsa());
        std::string path =
          Concatenate({GetAndroidRoot(), "/framework/oat/", arch, "/", name, extension});
        if (!OS::FileExists(path.c_str(), true)) {
          LOG(INFO) << "Missing " << path;
          return false;
        }
      }
    }
    return true;
  }

  bool CheckBootExtensions(const InstructionSet isa) const {
    // odrefresh compiles boot extensions as single image so there is only one output file of each
    // type (.art, .oat, .vdex).
    const std::string image_path = GetBootImageExtensionImageLocation(isa);
    int missing_outputs = 0;
    for (const auto& extension : {"art", "oat", "vdex"}) {
      std::string output_path = ReplaceFileExtension(image_path, extension);
      if (!OS::FileExists(output_path.c_str(), true)) {
        LOG(INFO) << "Missing " << output_path;
        missing_outputs++;
      }
    }
    return missing_outputs == 0;
  }

  static bool GetFreeSpace(const char* path, uint64_t* bytes) {
    struct statvfs sv;
    if (statvfs(path, &sv) != 0) {
      PLOG(ERROR) << "statvfs '" << path << "'";
      return false;
    }
    *bytes = sv.f_bfree * sv.f_bsize;
    return true;
  }

  static bool GetUsedSpace(const char* path, uint64_t* bytes) {
    *bytes = 0;

    std::queue<std::string> unvisited;
    unvisited.push(path);
    while (!unvisited.empty()) {
      std::string current = unvisited.front();
      std::unique_ptr<DIR, int (*)(DIR*)> dir(opendir(current.c_str()), closedir);
      for (auto entity = readdir(dir.get()); entity != nullptr; entity = readdir(dir.get())) {
        if (entity->d_name[0] == '.') {
          continue;
        }
        std::string entity_name = Concatenate({current, "/", entity->d_name});
        if (entity->d_type == DT_DIR) {
          unvisited.push(entity_name.c_str());
        } else if (entity->d_type == DT_REG) {
          // RoundUp file size to number of blocks.
          *bytes += RoundUp(OS::GetFileSizeBytes(entity_name.c_str()), 512);
        } else {
          LOG(FATAL) << "Unsupported directory entry type: " << static_cast<int>(entity->d_type);
        }
      }
      unvisited.pop();
    }
    return true;
  }

  static void ReportSpace() {
    uint64_t bytes;
    std::string data_dir = GetArtApexData();
    if (GetUsedSpace(data_dir.c_str(), &bytes)) {
      LOG(INFO) << "Used space " << bytes << " bytes.";
    }
    if (GetFreeSpace(data_dir.c_str(), &bytes)) {
      LOG(INFO) << "Available space " << bytes << " bytes.";
    }
  }

  int CheckArtifacts() {
    if (IsFactoryApex(config_.GetApexInfoListFile().c_str())) {
      // TODO(oth): temporary check just for progress.
      LOG(INFO) << "CheckArtifacts: factory boot extensions " << CheckBootExtensionsOnSystem();
      LOG(INFO) << "CheckArtifacts: factory systemserver " << CheckSystemServerOnSystem();
    }

    // TODO(oth): Factor available space into compilation logic.
    ReportSpace();

    for (const InstructionSet isa : config_.GetBootExtensionIsas()) {
      if (!CheckBootExtensions(isa)) {
        return ExitCode::kCompilationRequired;
      }
    }
    if (!CheckSystemServerJars()) {
      return ExitCode::kCompilationRequired;
    }

    return ExitCode::kOkay;
  }

  static int RemoveArtifact(const char* fpath, const struct stat*, int typeflag, struct FTW* ftw) {
    switch (typeflag) {
      case FTW_F:
      case FTW_SL:
      case FTW_SLN:
        if (unlink(fpath)) {
          PLOG(FATAL) << "Failed unlink(\"" << fpath << "\")";
        }
        return 0;

      case FTW_DP:
        if (ftw->level == 0) {
          return 0;
        }
        if (rmdir(fpath) != 0) {
          PLOG(FATAL) << "Failed rmdir(\"" << fpath << "\")";
        }
        return 0;

      case FTW_DNR:
        LOG(FATAL) << "Inaccessible directory \"" << fpath << "\"";
        return -1;

      case FTW_NS:
        LOG(FATAL) << "Failed stat() \"" << fpath << "\"";
        return -1;

      default:
        LOG(FATAL) << "Unexpected typeflag " << typeflag << "for \"" << fpath << "\"";
        return -1;
    }
  }

  void RemoveArtifactsOrDie() const {
    // Remove everything under ArtApexDataDir
    std::string data_dir = GetArtApexData();

    // Perform depth first traversal removing artifacts.
    nftw(data_dir.c_str(), RemoveArtifact, 1, FTW_DEPTH | FTW_MOUNT);
  }

  void RemoveStagingFilesOrDie(const char* staging_dir) const {
    if (OS::DirectoryExists(staging_dir)) {
      nftw(staging_dir, RemoveArtifact, 1, FTW_DEPTH | FTW_MOUNT);
    }
  }

  static bool IsCompilableBootExtension(const std::string_view& jar_path) {
    // Jar files in com.android.i18n are considered compilable because the APEX is not updatable.
    if (StartsWith(jar_path, "/apex/com.android.i18n")) {
      return true;
    }

    // Do not compile extensions from updatable APEXes.
    return !StartsWith(jar_path, "/apex");
  }

  static bool IsCompilableSystemServerJar(const std::string_view& jar_path) {
    // Do not compile jar files from updatable APEXes.
    return !StartsWith(jar_path, "/apex");
  }

  // Create all directory and all required parents.
  static void EnsureDirectoryExists(std::string_view absolute_path) {
    CHECK(absolute_path.size() > 0 && absolute_path[0] == '/');
    std::string path;
    VisitTokens(absolute_path, '/', [&path](std::string_view directory) {
      path.append("/").append(directory);
      if (!OS::DirectoryExists(path.c_str())) {
        if (mkdir(path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
          PLOG(FATAL) << "Could not create directory: " << path.c_str();
        }
      }
    });
  }

  static std::string GetBootImage() {
    // Typically /apex/com.android.art/javalib/boot.art.
    return GetArtRoot() + "/javalib/boot.art";
  }

  std::string GetBootImageExtensionImage(bool system = false) const {
    CHECK(!boot_extension_compilable_jars_.empty());
    std::string_view basename = Basename(boot_extension_compilable_jars_[0], ".jar");
    if (system) {
      // Typically /system/framework/boot-framework.art.
      return Concatenate({GetAndroidRoot(), "framework/boot-", basename, ".art"});
    } else {
      // Typically /data/misc/apexdata/system/framework/boot-framework.art.
      return Concatenate({boot_extension_output_dir_, "/boot-", basename, ".art"});
    }
  }

  std::string GetBootImageExtensionImageLocation(const InstructionSet isa) const {
    // Typically /data/misc/apexdata/system/framework/<arch>/boot-framework.art".
    CHECK(!boot_extension_compilable_jars_.empty());
    const char* isa_str = GetInstructionSetString(isa);
    std::string_view basename = Basename(boot_extension_compilable_jars_[0], ".jar");
    return Concatenate({boot_extension_output_dir_, "/", isa_str, "/boot-", basename, ".art"});
  }

  std::string GetSystemServerAppImageLocation(const std::string& jar_path) const {
    // Typically /data/misc/apexdata/system/framework/oat/<systemserver_arch>/<jar_name>.art.
    std::string_view basename = Basename(jar_path, ".jar");
    const char* isa_str = GetInstructionSetString(config_.GetSystemServerIsa());
    return Concatenate({systemserver_output_dir_, "/", isa_str, "/", basename, ".art"});
  }

  std::string StagingLocation(const std::string_view& staging_dir,
                              const std::string_view& path) const {
    return Concatenate({staging_dir, "/", Basename(path)});
  }

  bool CompileBootExtensions(const InstructionSet isa,
                             std::string_view staging_dir,
                             std::string* error_msg) const {
    std::vector<std::string> args;
    args.push_back(config_.GetDex2Oat());

    // Convert davlik.vm.boot-dex2oat-* properties to arguments.
    AddDex2OatBootTimeArguments(&args);

    // Add optional arguments.
    const std::string boot_profile_file(GetAndroidRoot() + "/etc/boot-image.prof");
    if (OS::FileExists(boot_profile_file.c_str())) {
      args.emplace_back(Concatenate({"--profile-file=", boot_profile_file}));
    } else {
      LOG(WARNING) << "Missing boot image profile: " << QuotePath(boot_profile_file);
    }
    const std::string dirty_image_objects_file(GetAndroidRoot() + "/etc/dirty-image-objects");
    if (OS::FileExists(dirty_image_objects_file.c_str())) {
      args.emplace_back(Concatenate({"--dirty-image-objects=", dirty_image_objects_file}));
    } else {
      LOG(WARNING) << "Missing dirty objects file : " << QuotePath(dirty_image_objects_file);
    }

    // Set boot-image and expectation of compiling boot classpath extensions.
    args.emplace_back("--boot-image=" + GetBootImage());

    // Add boot extensions to compile.
    for (const std::string& component : boot_extension_compilable_jars_) {
      args.emplace_back("--dex-file=" + component);
    }

    args.emplace_back("--runtime-arg");
    args.emplace_back(
      "-Xbootclasspath:" + android::base::Join(config_.GetDex2oatBootclasspath(), ':'));
    args.emplace_back("--avoid-storing-invocation");
    args.emplace_back("--compiler-filter=speed-profile");
    args.emplace_back("--generate-debug-info");
    args.emplace_back("--image-format=lz4hc");
    args.emplace_back("--strip");
    args.emplace_back("--boot-image=" + GetBootImage());
    args.emplace_back("--android-root=out/empty");
    args.emplace_back("--abort-on-hard-verifier-error");
    args.emplace_back(Concatenate({"--instruction-set=", GetInstructionSetString(isa)}));
    args.emplace_back("--generate-mini-debug-info");

    // Compile as a single image for fewer files and slightly less memory overhead.
    args.emplace_back("--single-image");

    const std::string image_path = GetBootImageExtensionImageLocation(isa);
    const std::string oat_path = ReplaceFileExtension(image_path, "oat");
    const std::string vdex_path = ReplaceFileExtension(image_path, "vdex");

    args.emplace_back("--oat-location=" + oat_path);

    std::pair<const char*, const char*> path_kind_pairs[] = {
      std::make_pair(image_path.c_str(), "image"),
      std::make_pair(oat_path.c_str(), "oat"),
      std::make_pair(vdex_path.c_str(), "output-vdex")
    };

    std::vector<std::unique_ptr<File>> output_files;
    for (const auto& path_kind_pair : path_kind_pairs) {
      auto [path, kind] = path_kind_pair;
      std::unique_ptr<File> file(OS::CreateEmptyFile(StagingLocation(staging_dir, path).c_str()));
      if (file == nullptr) {
        PLOG(ERROR) << "Failed to create " << kind << "file: " << path;
        EraseFiles(output_files);
        return false;
      }
      args.emplace_back(android::base::StringPrintf("--%s-fd=%d", kind, file->Fd()));
      output_files.emplace_back(std::move(file));
    }

    EnsureDirectoryExists(Dirname(image_path));

    std::string cmd_line = android::base::Join(args, ' ');
    LOG(INFO) << "Compiling boot extensions (" << isa << "): " << cmd_line;
    if (config_.GetDryRun()) {
      return true;
    }

    if (!Exec(args, error_msg)) {
      EraseFiles(output_files);
      return false;
    }

    if (!MoveOrEraseFiles(output_files, Dirname(image_path))) {
      return false;
    }

    return true;
  }

  bool CompileSystemServerJars(const std::string_view staging_dir, std::string* error_msg) const {
    std::vector<std::string> classloader_context;

    for (const std::string& jar : systemserver_compilable_jars_) {
      std::vector<std::string> args;
      args.emplace_back(config_.GetDex2Oat());

      // Convert davlik.vm.boot-dex2oat-* properties to arguments.
      AddDex2OatBootTimeArguments(&args);

      args.emplace_back("--dex-file=" + jar);

      const std::string app_image_path = GetSystemServerAppImageLocation(jar);
      const std::string oat_path = ReplaceFileExtension(app_image_path, "odex");
      const std::string vdex_path = ReplaceFileExtension(app_image_path, "vdex");

      if (classloader_context.empty()) {
        // All images are in the same directory, only need to check for first iteration.
        EnsureDirectoryExists(Dirname(app_image_path));
      }

      std::vector<std::unique_ptr<File>> output_files;

      std::pair<const char*, const char*> path_kind_pairs[] = {
        std::make_pair(app_image_path.c_str(), "app-image"),
        std::make_pair(oat_path.c_str(), "oat"),
        std::make_pair(vdex_path.c_str(), "output-vdex")
      };

      for (const auto& path_kind_pair : path_kind_pairs) {
        auto [path, kind] = path_kind_pair;
        std::unique_ptr<File> file(OS::CreateEmptyFile(StagingLocation(staging_dir, path).c_str()));
        if (file == nullptr) {
          PLOG(ERROR) << "Failed to create " << kind << " file: " << path;
          EraseFiles(output_files);
          return false;
        }
        args.emplace_back(android::base::StringPrintf("--%s-fd=%d", kind, file->Fd()));
        output_files.emplace_back(std::move(file));
      }
      args.emplace_back("--oat-location=" + oat_path);

      std::string jar_name(Basename(jar));
      const std::string profile_file =
        Concatenate({GetAndroidRoot(), "framework/", jar_name, ".prof"});
      if (OS::FileExists(profile_file.c_str())) {
        args.emplace_back("--profile-file=" + profile_file);
        args.emplace_back("--compiler-filter=speed-profile");
      } else {
        args.emplace_back("--compiler-filter=speed");
      }

      if (!config_.GetUpdatableBcpPackagesFile().empty()) {
        args.emplace_back("--updatable-bcp-packages-file=" + config_.GetUpdatableBcpPackagesFile());
      }

      const char* isa_str = GetInstructionSetString(config_.GetSystemServerIsa());
      args.emplace_back(Concatenate({"--instruction-set=", isa_str}));

      args.emplace_back("--runtime-arg");
      args.emplace_back(
        "-Xbootclasspath:" + android::base::Join(config_.GetDex2oatBootclasspath(), ':'));
      args.emplace_back("--avoid-storing-invocation");
      args.emplace_back(
        "--class-loader-context=PCL[" + android::base::Join(classloader_context, ':') + "]");
      args.emplace_back(
        Concatenate({"--boot-image=", GetBootImage(), ":", GetBootImageExtensionImage()}));

      args.emplace_back("--android-root=out/empty");
      args.emplace_back("--abort-on-hard-verifier-error");
      args.emplace_back("--generate-mini-debug-info");
      args.emplace_back("--compilation-reason=boot");
      args.emplace_back("--image-format=lz4hc");
      args.emplace_back("--resolve-startup-const-strings=true");

      LOG(INFO) << "Compiling " << jar << ": " << android::base::Join(args, ' ');
      if (config_.GetDryRun()) {
        return true;
      }

      if (!Exec(args, error_msg)) {
        EraseFiles(output_files);
        return false;
      }

      if (!MoveOrEraseFiles(output_files, Dirname(app_image_path))) {
        return false;
      }

      classloader_context.emplace_back(jar);
    }

    return true;
  }

  int Compile() const {
    // Clean-up existing files.
    RemoveArtifactsOrDie();

    // Create staging area and assign label for generating compilation artifacts.
    const char* staging_dir;
    if (PaletteCreateOdrefreshStagingDirectory(&staging_dir) != PaletteStatus::kOkay) {
      return ExitCode::kCompilationFailed;
    }

    std::string error_msg;

    for (const InstructionSet isa : config_.GetBootExtensionIsas()) {
      if (!CompileBootExtensions(isa, staging_dir, &error_msg)) {
        LOG(ERROR) << "BCP compilation failed: " << error_msg;
        RemoveStagingFilesOrDie(staging_dir);
        return ExitCode::kCompilationFailed;
      }
    }

    if (!CompileSystemServerJars(staging_dir, &error_msg)) {
      LOG(ERROR) << "system_server compilation failed: " << error_msg;
      RemoveStagingFilesOrDie(staging_dir);
      return ExitCode::kCompilationFailed;
    }

    // Staging area should have no files, but remove just in case.
    RemoveStagingFilesOrDie(staging_dir);

    return ExitCode::kOkay;
  }

  static bool ArgumentMatches(const char* arg, const char* prefix, std::string* value) {
    if (StartsWith(arg, prefix)) {
      *value = std::string(arg + strlen(prefix));
      return true;
    }
    return false;
  }

  static bool ArgumentEquals(const char* arg, const char* expected) {
    return strcmp(arg, expected) == 0;
  }

  static int InitializeHostConfig(int argc, const char** argv, OdrConfig* config) {
    std::string current_binary;
    if (argv[0][0] == '/') {
      current_binary = argv[0];
    } else {
      std::vector<char> buf(PATH_MAX);
      if (getcwd(buf.data(), buf.size()) == nullptr) {
        PLOG(FATAL) << "Failed getwd()";
      }
      current_binary = Concatenate({buf.data(), "/", argv[0]});
    }
    config->SetArtBinDir(std::string(Dirname(current_binary)));

    int n = 1;
    for (; n < argc - 1; ++n) {
      const char* arg = argv[n];
      std::string value;
      if (ArgumentMatches(arg, "--android-root=", &value)) {
        setenv("ANDROID_ROOT", value.c_str(), 1);
        config->SetAndroidRoot(value);
      } else if (ArgumentMatches(arg, "--android-art-root=", &value)) {
        setenv("ANDROID_ART_ROOT", value.c_str(), 1);
      } else if (ArgumentMatches(arg, "--apex-info-list=", &value)) {
        config->SetApexInfoListFile(value);
      } else if (ArgumentMatches(arg, "--art-apex-data=", &value)) {
        setenv("ART_APEX_DATA", value.c_str(), 1);
      } else if (ArgumentMatches(arg, "--dex2oat-bootclasspath=", &value)) {
        config->SetDex2oatBootclasspath(value);
      } else if (ArgumentEquals(arg, "--dry-run")) {
        config->SetDryRun();
      } else if (ArgumentMatches(arg, "--isa=", &value)) {
        config->SetIsa(GetInstructionSetFromString(value.c_str()));
      } else if (ArgumentMatches(arg, "--system-server-classpath=", &value)) {
        config->SetSystemServerClasspath(arg);
      } else if (ArgumentMatches(arg, "--updatable-bcp-packages-file=", &value)) {
        config->SetUpdatableBcpPackagesFile(value);
      } else if (ArgumentMatches(arg, "--zygote-arch=", &value)) {
        ZygoteKind zygote_kind;
        if (!ParseZygoteKind(value.c_str(), &zygote_kind)) {
          ArgumentError("Unrecognized zygote kind: '%s'", value.c_str());
        }
        config->SetZygoteKind(zygote_kind);
      } else {
        UsageError("Unrecognized argument: '%s'", arg);
      }
    }
    return n;
  }

  static void InitializeTargetConfig(OdrConfig* config) {
    config->SetAndroidRoot(art::GetAndroidRoot());
    config->SetApexInfoListFile("/apex/apex-info-list.xml");
    config->SetArtBinDir(art::GetArtBinDir());
    config->SetDex2oatBootclasspath(getenv("DEX2OATBOOTCLASSPATH"));
    config->SetSystemServerClasspath(getenv("SYSTEMSERVERCLASSPATH"));

    std::string abi = android::base::GetProperty("ro.product.cpu.abi", {});
    InstructionSet is = GetInstructionSetFromString(abi.c_str());
    if (is == InstructionSet::kNone) {
      LOG(FATAL) << "Unknown abi: '" << abi << "'";
    }
    config->SetIsa(is);

    std::string zygote = android::base::GetProperty("ro.zygote", {});
    ZygoteKind zygote_kind;
    if (!ParseZygoteKind(zygote.c_str(), &zygote_kind)) {
      LOG(FATAL) << "Unknown zygote: '" << zygote << "'";
    }
    config->SetZygoteKind(zygote_kind);

    std::string updatable_packages =
      android::base::GetProperty("dalvik.vm.dex2oat-updatable-bcp-packages-file", {});
    config->SetUpdatableBcpPackagesFile(updatable_packages);
  }

  static int main(int argc, const char** argv) {
    OdrConfig config;
    if (kIsTargetBuild) {
      InitializeTargetConfig(&config);
      argv += 1;
      argc -= 1;
    } else {
      __android_log_set_logger(__android_log_stderr_logger);
      int n = InitializeHostConfig(argc, argv, &config);
      argv += n;
      argc -= n;
    }

    if (argc != 1) {
      UsageError("Expected 1 argument, but have %d.", argc);
    }

    OnDeviceRefresh odr(config);
    for (int i = 0; i < argc; ++i) {
      std::string_view action(argv[i]);
      if (action == "--check") {
        // TODO(oth): remove this check, check artifacts on /system as they may be incomplete from
        // factory by design.
        if (odr.IsFactoryApex(config.GetApexInfoListFile().c_str())) {
          return ExitCode::kOkay;
        }
        return odr.CheckArtifacts();
      } else if (action == "--force-check") {
        return odr.CheckArtifacts();
      } else if (action == "--compile") {
        if (odr.IsFactoryApex(config.GetApexInfoListFile().c_str())) {
          return ExitCode::kOkay;
        }
        // TODO(oth): check freshness of artifacts before compiling. Can /system artifacts be used?
        return odr.Compile();
      } else if (action == "--force-compile") {
        return odr.Compile();
      } else if (action == "--help") {
        UsageHelp(argv[0]);
      } else {
        UsageError("Unknown argument: ", argv[i]);
      }
    }
    return ExitCode::kOkay;
  }

  DISALLOW_COPY_AND_ASSIGN(OnDeviceRefresh);
};

}  // namespace odrefresh
}  // namespace art

int main(int argc, const char** argv) {
  return art::odrefresh::OnDeviceRefresh::main(argc, argv);
}
