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

#include "odr_dexopt.h"

#include <vector>

#include "android-base/logging.h"
#include "android-base/parseint.h"
#include "android-base/strings.h"
#include "exec_utils.h"
#include "log/log.h"
#include "odr_config.h"
#include "libdexopt.h"

#include "aidl/com/android/art/DexoptBcpExtArgs.h"
#include "aidl/com/android/art/DexoptSystemServerArgs.h"

namespace art {
namespace odrefresh {

using aidl::com::android::art::DexoptBcpExtArgs;
using aidl::com::android::art::DexoptSystemServerArgs;

namespace {

int ExecAndReturnCode(ExecUtils* exec_utils,
                      std::vector<std::string>& cmdline,
                      time_t timeout_secs,
                      /*out*/ bool* timed_out,
                      /*out*/ std::string* error_msg) {
  LOG(DEBUG) << "odr_dexopt cmdline: " << android::base::Join(cmdline, ' ') << " [timeout "
             << timeout_secs << "s]";
  return exec_utils->ExecAndReturnCode(cmdline, timeout_secs, timed_out, error_msg);
}

class OdrDexoptLocal final : public OdrDexopt::Delegate {
 public:
  static OdrDexoptLocal* Create(const std::string& dex2oat_path, ExecUtils* exec_utils) {
    return new OdrDexoptLocal(dex2oat_path, exec_utils);
  }

  int DexoptBcpExtension(const DexoptBcpExtArgs& args,
                         time_t timeout_secs,
                         /*out*/ bool* timed_out,
                         /*out*/ std::string* error_msg) override {
    std::vector<std::string> cmdline = { dex2oat_path_ };
    if (!art::AddDex2oatArgsFromBcpExtensionArgs(args, cmdline)) {
      return -1;
    }
    return ExecAndReturnCode(exec_utils_, cmdline, timeout_secs, timed_out, error_msg);
  }

  int DexoptSystemServer(const DexoptSystemServerArgs& args,
                         time_t timeout_secs,
                         /*out*/ bool* timed_out,
                         /*out*/ std::string* error_msg) override {
    std::vector<std::string> cmdline = { dex2oat_path_ };
    if (!art::AddDex2oatArgsFromSystemServerArgs(args, cmdline)) {
      return -1;
    }
    return ExecAndReturnCode(exec_utils_, cmdline, timeout_secs, timed_out, error_msg);
  }

 private:
  OdrDexoptLocal(const std::string& dex2oat_path, ExecUtils* exec_utils)
      : dex2oat_path_(dex2oat_path), exec_utils_(exec_utils) {}

  std::string dex2oat_path_;
  ExecUtils* exec_utils_;  // not owned
};

class OdrDexoptCompilationOS final : public OdrDexopt::Delegate {
 public:
  static OdrDexoptCompilationOS* Create(int cid, ExecUtils* exec_utils) {
    return new OdrDexoptCompilationOS(cid, exec_utils);
  }

  int DexoptBcpExtension(const DexoptBcpExtArgs& args,
                         time_t timeout_secs,
                         /*out*/ bool* timed_out,
                         /*out*/ std::string* error_msg) override {
    std::vector<int> input_fds, output_fds;
    collectFdsFromDexoptBcpExtensionArgs(input_fds, output_fds, args);

    std::vector<std::string> cmdline;
    AppendPvmExecArgs(cmdline, input_fds, output_fds);

    // Original dex2oat flags
    cmdline.push_back("/apex/com.android.art/bin/dex2oat64");
    if (!AddDex2oatArgsFromBcpExtensionArgs(args, cmdline)) {
      return -1;
    }

    return ExecAndReturnCode(exec_utils_, cmdline, timeout_secs, timed_out, error_msg);
  }

  int DexoptSystemServer(const DexoptSystemServerArgs& args,
                         time_t timeout_secs,
                         /*out*/ bool* timed_out,
                         /*out*/ std::string* error_msg) override {
    std::vector<int> input_fds, output_fds;
    collectFdsFromDexoptSystemServerArgs(input_fds, output_fds, args);

    std::vector<std::string> cmdline;
    AppendPvmExecArgs(cmdline, input_fds, output_fds);

    // Original dex2oat flags
    cmdline.push_back("/apex/com.android.art/bin/dex2oat64");
    if (!AddDex2oatArgsFromSystemServerArgs(args, cmdline)) {
      return -1;
    }

    LOG(DEBUG) << "DexoptSystemServer cmdline: " << android::base::Join(cmdline, ' ')
               << " [timeout " << timeout_secs << "s]";
    return ExecAndReturnCode(exec_utils_, cmdline, timeout_secs, timed_out, error_msg);
  }

 private:
  OdrDexoptCompilationOS(int cid, ExecUtils* exec_utils) : cid_(cid), exec_utils_(exec_utils) {}

  void AppendPvmExecArgs(/*inout*/ std::vector<std::string>& cmdline,
                         const std::vector<int>& input_fds,
                         const std::vector<int>& output_fds) {
    cmdline.emplace_back("/apex/com.android.compos/bin/pvm_exec");
    cmdline.emplace_back("--cid=" + std::to_string(cid_));
    cmdline.emplace_back("--in-fd=" + android::base::Join(input_fds, ','));
    cmdline.emplace_back("--out-fd=" + android::base::Join(output_fds, ','));
    cmdline.emplace_back("--");
  }

  void collectFdsFromDexoptBcpExtensionArgs(/*inout*/ std::vector<int>& input_fds,
                                            /*inout*/ std::vector<int>& output_fds,
                                            const DexoptBcpExtArgs& args) {
    // input
    insertOnlyNonNegative(input_fds, args.dexFds);
    insertIfNonNegative(input_fds, args.profileFd);
    insertIfNonNegative(input_fds, args.dirtyImageObjectsFd);
    insertOnlyNonNegative(input_fds, args.bootClasspathFds);
    // output
    insertIfNonNegative(output_fds, args.imageFd);
    insertIfNonNegative(output_fds, args.vdexFd);
    insertIfNonNegative(output_fds, args.oatFd);
  }

  void collectFdsFromDexoptSystemServerArgs(/*inout*/ std::vector<int>& input_fds,
                                            /*inout*/ std::vector<int>& output_fds,
                                            const DexoptSystemServerArgs& args) {
    // input
    insertIfNonNegative(input_fds, args.dexFd);
    insertIfNonNegative(input_fds, args.profileFd);
    insertIfNonNegative(input_fds, args.updatableBcpPackagesTxtFd);
    insertOnlyNonNegative(input_fds, args.bootClasspathFds);
    insertOnlyNonNegative(input_fds, args.bootClasspathImageFds);
    insertOnlyNonNegative(input_fds, args.bootClasspathVdexFds);
    insertOnlyNonNegative(input_fds, args.bootClasspathOatFds);
    insertOnlyNonNegative(input_fds, args.classloaderFds);
    // output
    insertIfNonNegative(output_fds, args.imageFd);
    insertIfNonNegative(output_fds, args.vdexFd);
    insertIfNonNegative(output_fds, args.oatFd);
  }

  void insertIfNonNegative(/*inout*/ std::vector<int>& vec, int n) {
    if (n < 0) {
      return;
    }
    vec.emplace_back(n);
  }

  void insertOnlyNonNegative(/*inout*/ std::vector<int>& vec, const std::vector<int>& ns) {
    std::copy_if(ns.begin(), ns.end(), std::back_inserter(vec), [](int n) { return n >= 0; });
  }

  int cid_;
  ExecUtils* exec_utils_;  // not owned
};

}  // namespace

// static
std::unique_ptr<OdrDexopt> OdrDexopt::Create(const OdrConfig& config, ExecUtils* exec_utils) {
  auto self = std::make_unique<OdrDexopt>();
  if (config.UseCompilationOs()) {
    int cid;
    if (!android::base::ParseInt(config.GetCompilationOsAddress(), &cid)) {
      return nullptr;
    }
    self->delegate_.reset(OdrDexoptCompilationOS::Create(cid, exec_utils));
  } else {
    self->delegate_.reset(OdrDexoptLocal::Create(config.GetDex2Oat(), exec_utils));
  }
  return self;
}

int OdrDexopt::DexoptBcpExtension(const DexoptBcpExtArgs& args,
                                  time_t timeout_secs,
                                  /*out*/ bool* timed_out,
                                  /*out*/ std::string* error_msg) {
  return delegate_->DexoptBcpExtension(args, timeout_secs, timed_out, error_msg);
}

int OdrDexopt::DexoptSystemServer(const DexoptSystemServerArgs& args,
                                  time_t timeout_secs,
                                  /*out*/ bool* timed_out,
                                  /*out*/ std::string* error_msg) {
  return delegate_->DexoptSystemServer(args, timeout_secs, timed_out, error_msg);
}

}  // namespace odrefresh
}  // namespace art
