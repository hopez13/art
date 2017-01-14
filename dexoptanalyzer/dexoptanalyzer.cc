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

#include <string>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "compiler_filter.h"
#include "dex_file.h"
#include "noop_compiler_callbacks.h"
#include "oat_file_assistant.h"
#include "os.h"
#include "runtime.h"
#include "thread-inl.h"
#include "utils.h"

namespace art {

// The base value to be added to the result of OatFileAssistant::GetDexOptNeeded
// before returning.
// GetDexOptNeeded returns negative values for odex locations. This does not play
// nice with the usual macros used to process the results of commands (WIFEXITED
// and WEXITSTATUS) which look at the lower 8 bits. Use a large base which will
// be added to the return value so that we don't have to special case the execution
// of dexoptanalyzer. The caller will need to subtract this value from the command
// result to get the real GetDexOptNeeded result.
static int RETURN_CODE_BASE = 100;

// Return code for when the runtime cannot be created.
// Pick a large number to make sure it doesn't collide
// with valid returns (OatFileAssistant::DexOptNeeded).
static int ERROR_INVALID_ARGUMENTS = 100;
static int ERROR_CANNOT_CREATE_RUNTIME = 101;

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < original_argc; ++i) {
    command.push_back(original_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void Usage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("  Performs a dexopt analysis on the given dex file and returns whether or not");
  UsageError("  the dex file needs to be dexopted.");
  UsageError("Usage: dexoptanalyzer [options]...");
  UsageError("");
  UsageError("  --dex-file=<filename>: the dex file which should be analyzed.");
  UsageError("");
  UsageError("  --isa=<string>: the instruction set for which the analysis should be performed.");
  UsageError("");
  UsageError("  --compiler-filter=<string>: the target compiler filter to be used as reference");
  UsageError("       when deciding if the dex file needs to be optimized.");
  UsageError("");
  UsageError("  --assume-profile-changed: assumes the profile information has changed");
  UsageError("       when deciding if the dex file needs to be optimized.");
  UsageError("  --image=<filename>: optional, the image to be used to decide if the associated");
  UsageError("       oat file is up to date. By default the image file is constructed based");
  UsageError("       on ANDROID_ROOT env variable.");
  UsageError("       Example: --image=/system/framework/boot.art");
  UsageError("");
  UsageError("  --android-data=<directory>: optional, the directory which should be used as");
  UsageError("       android-data. By default ANDROID_DATA env variable is used.");
  UsageError("");
  UsageError("Return code:");
  UsageError("  If successful, dexoptanalyzer returns one of OatFileAssistant::DexOptNeeded codes");
  UsageError("    shifted by the value 100 (referred as RETURN_CODE_BASE). To retrieve the");
  UsageError("    DexOptNeeded status the caller must thus subtract RETURN_CODE_BASE from the");
  UsageError("    return code. Refer to OatFileAssistant::DexOptNeeded for the actual values.");
  UsageError("  In case of error, the return code is 200 (ERROR_CANNOT_CREATE_RUNTIME).");
  UsageError("");

  exit(ERROR_INVALID_ARGUMENTS);
}

class DexoptAnalyzer FINAL {
 public:
  DexoptAnalyzer() : asssume_profile_changed_(false) {}

  void ParseArgs(int argc, char **argv) {
    original_argc = argc;
    original_argv = argv;

    InitLogging(argv, Runtime::Aborter);
    // Skip over the command name.
    argv++;
    argc--;

    if (argc == 0) {
      Usage("No arguments specified");
    }

    for (int i = 0; i < argc; ++i) {
      const StringPiece option(argv[i]);
      if (option == "--assume-profile-changed") {
        asssume_profile_changed_ = true;
      } else if (option.starts_with("--dex-file=")) {
        dex_file_ = option.substr(strlen("--dex-file=")).ToString();
      } else if (option.starts_with("--compiler-filter=")) {
        std::string filter_str = option.substr(strlen("--compiler-filter=")).ToString();
        if (!CompilerFilter::ParseCompilerFilter(filter_str.c_str(), &compiler_filter_)) {
          Usage("Invalid compiler filter '%s'", option.data());
        }
      } else if (option.starts_with("--isa=")) {
          std::string isa_str = option.substr(strlen("--isa=")).ToString();
          isa_ = GetInstructionSetFromString(isa_str.c_str());
          if (isa_ == kNone) {
            Usage("Invalid isa '%s'", option.data());
          }
      } else if (option.starts_with("--image=")) {
        image_ = option.substr(strlen("--image=")).ToString();
      } else if (option.starts_with("--android-data=")) {
        // Overwrite android-data if needed (oat file assistant relies on a valid directory to
        // compute dalvik-cache folder). This is mostly used in tests.
        std::string new_android_data = option.substr(strlen("--android-data=")).ToString();
        setenv("ANDROID_DATA", new_android_data.c_str(), 1);
      } else {
        Usage("Unknown argument '%s'", option.data());
      }
    }

    if (image_.empty()) {
      // If we don't receive the image, try to use the default one.
      // Tests may specify a different image (e.g. core image).
      std::string error_msg;
      image_ = GetDefaultBootImageLocation(&error_msg);

      if (image_.empty()) {
        LOG(ERROR) << error_msg;
        Usage("--image unspecified and ANDROID_ROOT not set or image file does not exist.");
      }
    }
  }

  bool CreateRuntime() {
    RuntimeOptions options;
    // The image could be custom, so make sure we explicitly pass it.
    std::string img = "-Ximage:" + image_;
    options.push_back(std::make_pair(img.c_str(), nullptr));
    // The instruction set of the image should match the instruction set we will test.
    const void* isa_opt = reinterpret_cast<const void*>(GetInstructionSetString(isa_));
    options.push_back(std::make_pair("imageinstructionset", isa_opt));
     // Disable libsigchain. We don't don't need it to evaluate DexOptNeeded status.
    options.push_back(std::make_pair("-Xno-sig-chain", nullptr));
    // Pretend we are a compiler so that we can re-use the same infrastructure to load a different
    // ISA image and minimize the amount of things that get started.
    NoopCompilerCallbacks callbacks;
    options.push_back(std::make_pair("compilercallbacks", &callbacks));
    // Make sure we don't attempt to relocate. The tool should only retrieve the DexOptNeeded
    // status and not attempt to relocate the boot image.
    options.push_back(std::make_pair("-Xnorelocate", nullptr));

    if (!Runtime::Create(options, false)) {
      LOG(ERROR) << "Unable to initialize runtime";
      return false;
    }
    // Runtime::Create acquired the mutator_lock_ that is normally given away when we
    // Runtime::Start. Give it away now.
    Thread::Current()->TransitionFromRunnableToSuspended(kNative);

    return true;
  }

  int GetDexOptNeeded() {
    // If the file does not exist there's nothing to do.
    // This is a fast path to avoid creating the runtime (b/34385298).
    if (!OS::FileExists(dex_file_.c_str())) {
      return OatFileAssistant::kNoDexOptNeeded;
    }
    if (!CreateRuntime()) {
      return ERROR_CANNOT_CREATE_RUNTIME;
    }
    OatFileAssistant oat_file_assistant(dex_file_.c_str(), isa_, /*load_executable*/ false);
    // Always treat elements of the bootclasspath as up-to-date.
    // TODO(calin): this check should be in OatFileAssistant.
    if (oat_file_assistant.IsInBootClassPath()) {
      return OatFileAssistant::kNoDexOptNeeded;
    }
    return oat_file_assistant.GetDexOptNeeded(compiler_filter_, asssume_profile_changed_);
  }

 private:
  std::string dex_file_;
  InstructionSet isa_;
  CompilerFilter::Filter compiler_filter_;
  bool asssume_profile_changed_;
  std::string image_;
};

// Returns one of (RETURN_CODE_BASE + OatFileAssistant::DexOptNeeded) codes or
// (RETURN_CODE_BASE + ERROR_CANNOT_CREATE_RUNTIME) if the runtime could not be created.
static int dexoptAnalyze(int argc, char** argv) {
  DexoptAnalyzer analyzer;

  // Parse arguments. Argument mistakes will lead to exit(ERROR_INVALID_ARGUMENTS) in UsageError.
  analyzer.ParseArgs(argc, argv);
  return RETURN_CODE_BASE + analyzer.GetDexOptNeeded();
}

}  // namespace art

int main(int argc, char **argv) {
  return art::dexoptAnalyze(argc, argv);
}
