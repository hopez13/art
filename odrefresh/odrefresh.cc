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

#include <ftw.h>
#include <sysexits.h>
#include <sys/system_properties.h>

#include <cstdarg>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "base/file_utils.h"
#include "base/macros.h"
#include "base/os.h"
#include "base/string_view_cpp20.h"
#include "base/utils.h"
#include "exec_utils.h"

namespace art {

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

class OnDeviceRefresh {
 private:
  // Tool paths
  static constexpr const char* kDexOptAnalyzer = "/apex/com.android.art/bin/dexoptanalyzer";
  static constexpr const char* kDex2Oat32 = "/apex/com.android.art/bin/dex2oat32";
  static constexpr const char* kDex2Oat64 = "/apex/com.android.art/bin/dex2oat64";

  // Dex2Oat input paths
  static constexpr const char* kBootImageProfile = "/system/etc/boot-image.prof";
  static constexpr const char* kDirtyImageObjects = "/system/etc/dirty-image-objects";

  std::string dex2oat_;
  std::vector<std::string> bcp_archs_;
  std::string systemserver_arch_;
  std::string updatable_bcp_packages_file_;
  std::string dex2oat_bcp_;
  std::string systemserver_cp_;

 public:
  OnDeviceRefresh() {
    char value[PROP_VALUE_MAX];

    std::string arch32, arch64;
    GetProperty("ro.product.cpu.abi", value);
    if (StartsWith(value, "arm")) {
      arch32 = "arm";
      arch64 = "arm64";
    } else if (StartsWith(value, "x86")) {
      arch32 = "x86";
      arch64 = "x86_64";
    } else {
      LOG(FATAL) << "Unknown abi: " << value;
    }

    GetProperty("ro.zygote", value);
    std::string_view zygote_arch(value);
    if (zygote_arch == "zygote32") {
      bcp_archs_ = { arch32 };
      systemserver_arch_ = arch32;
      dex2oat_ = kDex2Oat32;
    } else if (zygote_arch == "zygote32_64") {
      bcp_archs_ = { arch32, arch64 };
      systemserver_arch_ = arch32;
      dex2oat_ = kDex2Oat64;
    } else if (zygote_arch == "zygote64_32") {
      bcp_archs_ = { arch32, arch64 };
      systemserver_arch_ = arch64;
      dex2oat_ = kDex2Oat64;
    } else if (zygote_arch == "zygote64") {
      bcp_archs_ = { arch64 };
      systemserver_arch_ = arch64;
      dex2oat_ = kDex2Oat64;
    } else {
      LOG(FATAL) << "Unknown zygote: " << zygote_arch;
    }

    if (GetOptionalProperty("dalvik.vm.dex2oat-updatable-bcp-packages-file", value)) {
      updatable_bcp_packages_file_ = value;
    }

    GetEnv("DEX2OATBOOTCLASSPATH", &dex2oat_bcp_);
    GetEnv("SYSTEMSERVERCLASSPATH", &systemserver_cp_);
  }

  static bool GetOptionalProperty(const char* name, char value[PROP_VALUE_MAX]) {
    value[0] = '\0';
    int length = __system_property_get(name, value);
    return length > 0 && length <= PROP_VALUE_MAX;
  }

  static void GetProperty(const char* name, char value[PROP_VALUE_MAX]) {
    if (!GetOptionalProperty(name, value)) {
      LOG(FATAL) << "Unable to get property \"" << name << "\"";
    }
  }

  static void GetEnv(const char* name, std::string* value) {
    const char* v = getenv(name);
    if (v == nullptr) {
      LOG(FATAL) << "Missing or invalid environment variable: " << name;
    }
    *value = v;
  }

  // Read apex_info_list.xml from input stream and determine if the ART APEX
  // listed is the factory installed version.
  bool IsFactoryInstalledArtApex(std::istream* is) const {
    std::string line;
    while (std::getline(*is, line)) {
      // TODO(oth): Check line matches regexps: <apex-info-list> | </apex-info-list>
      //                                        <apex-info>.*</apex-info>
      if (line.find("moduleName=\"com.android.art\"") == std::string::npos) {
        continue;
      }
      if (line.find("isActive=\"true\"") == std::string::npos) {
        continue;
      }
      return line.find("isFactory=\"true\"") != std::string::npos;
    }
    return false;
  }

  bool IsFactoryApex(const char* apex_info_list_xml_path = "/apex/apex-info-list.xml") const {
    std::ifstream ifs(apex_info_list_xml_path);
    return IsFactoryInstalledArtApex(&ifs);
  }

  int CheckArtifacts() {
    return 0;
  }

  static int RemoveArtifact(const char *fpath, const struct stat *, int typeflag, struct FTW *) {
    switch (typeflag) {
      case FTW_F:
      case FTW_SL:
      case FTW_SLN:
        if (unlink(fpath)) {
          PLOG(FATAL) << "Failed unlink(\"" << fpath << "\")";
        }
        return 0;

      case FTW_DP:
        if (strcmp(fpath, GetArtApexData().c_str()) == 0) {
          return 0;
        }
        if (rmdir(fpath)) {
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

  int RemoveArtifacts() {
    // Remove everything under ArtApexDataDir
    std::string data_dir = GetArtApexData();

    // Perform depth first traversal removing artifacts.
    return nftw(data_dir.c_str(), RemoveArtifact, /*nopenfd=*/ 4, FTW_DEPTH | FTW_MOUNT);
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
        if (mkdir(path.c_str(), /*RWXR--R--*/0744) != 0) {
          PLOG(FATAL) << "Could not create directory: " << path.c_str();
        }
      }
    });
  }

  bool CompileBootExtensions(const std::string& output_dir,
                             const std::string& isa,
                             std::string* error_msg) const {
    std::string output_isa_dir = output_dir + "/" + isa;
    EnsureDirectoryExists(output_isa_dir);

    std::ostringstream cmd;
    cmd << dex2oat_;

    // Add boot extensions to compile.
    VisitTokens(dex2oat_bcp_, ':', [&cmd](std::string_view component) {
      if (IsCompilableBootExtension(component)) {
        cmd << " --dex-file=" << component;
      }
    });
    cmd << " --runtime-arg -Xbootclasspath:" << dex2oat_bcp_.data();

    // Add optional arguments.
    if (OS::FileExists(kBootImageProfile)) {
      cmd << " --profile-file=" << kBootImageProfile;
    }
    if (OS::FileExists(kDirtyImageObjects)) {
      cmd << " --dirty-image-objects=" << kDirtyImageObjects;
    }

    cmd << " --avoid-storing-invocation"
        << " --compiler-filter=speed-profile"
        << " --generate-debug-info"
        << " --image-format=lz4hc"
        << " --strip"
        << " --boot-image=/apex/com.android.art/javalib/boot.art"
        << " --oat-file=" << output_isa_dir << "/boot.oat"
        << " --image=" << output_isa_dir << "/boot.art"
        << " --android-root=out/empty"
        << " --abort-on-hard-verifier-error"
        << " --instruction-set=" << isa
        << " --generate-mini-debug-info";

    std::string cmd_line = cmd.str();
    LOG(INFO) << "Compiling boot extensions (" << isa << "): " << cmd_line;

    std::vector<std::string> args;
    Split(cmd_line, ' ', &args);

    return Exec(args, error_msg);
  }

  bool CompileSystemServerJars(const std::string& output_dir,
                               std::string* error_msg) const {
    std::string output_isa_dir = output_dir + "/oat/" + systemserver_arch_;
    EnsureDirectoryExists(output_isa_dir);

    std::vector<std::string> systemserver_jars;
    VisitTokens(systemserver_cp_, ':', [&systemserver_jars](std::string_view component) {
      if (IsCompilableSystemServerJar(component)) {
        systemserver_jars.emplace_back(component);
      }
    });

    std::vector<std::string> classloader_context;

    for (const std::string& jar : systemserver_jars) {
      std::ostringstream cmd;
      cmd << dex2oat_;

      std::string stem(Basename(jar, ".jar"));
      std::string profile_file = android::base::StringPrintf("/system/framework/%s.jar.prof",
                                                             stem.c_str());
      if (OS::FileExists(profile_file.c_str())) {
        cmd << " --profile-file=" << profile_file;
        cmd << " --compiler-filter=speed-profile";
      } else {
        cmd << " --compiler-filter=speed";
      }

      if (!updatable_bcp_packages_file_.empty()) {
        cmd << " --updatable-bcp-packages-file=" << updatable_bcp_packages_file_;
      }

      cmd << " --avoid-storing-invocation"
          << " --runtime-arg -Xbootclasspath:" << dex2oat_bcp_
          << " --class-loader-context=PCL[" << android::base::Join(classloader_context, ':') << "]"
          << " --boot-image="
          <<     "/apex/com.android.art/javalib/boot.art:" << output_dir << "/boot-framework.art"
          << " --dex-file=" << jar
          << " --oat-file=" << output_isa_dir << '/' << stem << ".odex"
          << " --app-image-file=" << output_isa_dir << '/' << stem << ".art"
          << " --android-root=out/empty"
          << " --instruction-set=" << systemserver_arch_
          << " --abort-on-hard-verifier-error"
          << " --generate-mini-debug-info"
          << " --compilation-reason=prebuilt"
          << " --image-format=lz4"
          << " --resolve-startup-const-strings=true";

      std::string cmd_line = cmd.str();
      LOG(INFO) << "Compiling " << jar << ": " << cmd_line;

      std::vector<std::string> args;
      Split(cmd_line, ' ', &args);

      if (!Exec(args, error_msg)) {
        return false;
      }

      classloader_context.emplace_back(jar);
    }

    return true;
  }

  int Compile() const {
    std::string bcp_output_dir = GetArtApexData() + "/system/framework";
    std::string error_msg;

    for (const std::string& bcp_arch : bcp_archs_) {
      if (!CompileBootExtensions(bcp_output_dir, bcp_arch, &error_msg)) {
        LOG(ERROR) << "BCP compilation failed: " << error_msg;
        return -1;  // TODO(oth)
      }
    }

    std::string systemserver_output_dir = GetArtApexData() + "/system/framework/oat";
    if (!CompileSystemServerJars(systemserver_output_dir, &error_msg)) {
      LOG(ERROR) << "system_server compilation failed: " << error_msg;
      return -1;  // TODO(oth)
    }
    return 0;
  }

  static int main(int argc, const char** argv) {
    if (argc != 2) {
      ArgumentError("Unexpected arguments presented.");
    }

    OnDeviceRefresh odr;
    for (int i = 1; i < argc; ++i) {
      std::string_view action(argv[i]);
      if (action == "--check") {
        if (odr.IsFactoryApex()) {
          return EX_OK;
        }
        return odr.CheckArtifacts();
      } else if (action == "--compile") {
        if (odr.IsFactoryApex()) {
          return EX_OK;
        }
        // TODO(oth): check freshness of artifacts before compiling.
        return odr.Compile();
      } else if (action == "--force-compile") {
        int removalStatus = odr.RemoveArtifacts();
        if (removalStatus != 0) {
          return removalStatus;
        }
        return odr.Compile();
      } else if (action == "--help") {
        UsageHelp(argv[0]);
      } else {
        UsageError("Unknown argument: ", argv[i]);
      }
    }
    return EX_OK;
  }
};

}  // namespace art

int main(int argc, const char** argv) {
  return art::OnDeviceRefresh::main(argc, argv);
}
