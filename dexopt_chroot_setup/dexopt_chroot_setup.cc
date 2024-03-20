/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "dexopt_chroot_setup.h"

#include <linux/mount.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "aidl/com/android/server/art/BnDexoptChrootSetup.h"
#include "android-base/errors.h"
#include "android-base/file.h"
#include "android-base/logging.h"
#include "android-base/no_destructor.h"
#include "android-base/properties.h"
#include "android-base/result.h"
#include "android-base/strings.h"
#include "android/binder_auto_utils.h"
#include "android/binder_manager.h"
#include "android/binder_process.h"
#include "base/file_utils.h"
#include "base/macros.h"
#include "base/os.h"
#include "exec_utils.h"
#include "fstab/fstab.h"
#include "tools/binder_utils.h"
#include "tools/cmdline_builder.h"
#include "tools/tools.h"

namespace art {
namespace dexopt_chroot_setup {

namespace {

using ::android::base::ConsumePrefix;
using ::android::base::EndsWith;
using ::android::base::Error;
using ::android::base::Join;
using ::android::base::NoDestructor;
using ::android::base::ReadFileToString;
using ::android::base::Result;
using ::android::base::SetProperty;
using ::android::base::Split;
using ::android::base::StringReplace;
using ::android::base::Tokenize;
using ::android::base::WaitForProperty;
using ::android::fs_mgr::FstabEntry;
using ::art::tools::CmdlineBuilder;
using ::art::tools::Fatal;
using ::art::tools::GetProcMountsDescendantsOfPath;
using ::art::tools::NonFatal;
using ::art::tools::PathStartsWith;
using ::ndk::ScopedAStatus;

using std::literals::operator""s;  // NOLINT

constexpr const char* kServiceName = "dexopt_chroot_setup";
constexpr const char* kBindMountTmpDir = "/mnt/pre_reboot_dexopt/mount_tmp";
constexpr const char* kDeviceMapperDir = "/dev/block/mapper";
constexpr mode_t kMode = 0755;
constexpr std::chrono::milliseconds kSnapshotCtlTimeout = std::chrono::seconds(60);

Result<void> Run(std::string_view name, const std::vector<std::string>& args) {
  LOG(INFO) << "Running " << name << ": " << Join(args, /*separator=*/" ");

  std::string error_msg;
  if (!Exec(args, &error_msg)) {
    return Errorf("Failed to run {}: {}", name, error_msg);
  }

  LOG(INFO) << name << " returned code 0";
  return {};
}

Result<std::string> GetArtExec() {
  std::string error_msg;
  std::string art_root = GetArtRootSafe(&error_msg);
  if (!error_msg.empty()) {
    return Error() << error_msg;
  }
  return art_root + "/bin/art_exec";
}

Result<void> CreateDirs(const std::string& path) {
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    return Errorf("Failed to create dir '{}': {}", path, ec.message());
  }
  return {};
}

Result<void> BindMount(const std::string& source, const std::string& target) {
  if (PathStartsWith(source, DexoptChrootSetup::CHROOT_DIR)) {
    // Don't bind-mount repeatedly.
    return {};
  }
  // We want the propagation type to be "slave+shared": slave of `source` while at the same time
  // shared with other processes (e.g., system_server). This can be achieved in 3 steps:
  // 1. Bind-mount `source` at a temp mount point.
  // 2. Make the temp mount point slave.
  // 3. Bind-mount the temp mount point at `target`.
  OR_RETURN(CreateDirs(kBindMountTmpDir));
  if (mount(source.c_str(),
            kBindMountTmpDir,
            /*fs_type=*/nullptr,
            MS_BIND,
            /*data=*/nullptr) != 0) {
    return ErrnoErrorf("Failed to bind-mount '{}' at '{}'", source, kBindMountTmpDir);
  }
  if (mount(/*source=*/nullptr,
            kBindMountTmpDir,
            /*fs_type=*/nullptr,
            MS_SLAVE,
            /*data=*/nullptr) != 0) {
    return ErrnoErrorf("Failed to make mount slave for '{}'", kBindMountTmpDir);
  }
  if (mount(kBindMountTmpDir,
            target.c_str(),
            /*fs_type=*/nullptr,
            MS_BIND,
            /*data=*/nullptr) != 0) {
    return ErrnoErrorf("Failed to bind-mount '{}' at '{}'", kBindMountTmpDir, target);
  }
  if (umount2(kBindMountTmpDir, UMOUNT_NOFOLLOW) != 0) {
    return ErrnoErrorf("Failed to umount2 '{}'", kBindMountTmpDir);
  }
  LOG(INFO) << ART_FORMAT("Bind-mounted '{}' at '{}'", source, target);
  return {};
}

Result<void> BindMountRecursive(const std::string& source, const std::string& target) {
  CHECK(!EndsWith(source, '/'));
  OR_RETURN(BindMount(source, target));

  // Mount and make slave one by one. Do not use MS_REC because we don't want to mount a child if
  // the parent cannot be slave (i.e., is shared). Otherwise, unmount events will be undesirably
  // propagated to the source. For example, if "/dev" and "/dev/pts" are mounted at "/chroot/dev"
  // and "/chroot/dev/pts" respectively, and "/chroot/dev" is shared, then unmounting
  // "/chroot/dev/pts" will also unmount "/dev/pts".
  //
  // The list is in mount order.
  std::vector<FstabEntry> entries = OR_RETURN(GetProcMountsDescendantsOfPath(source));
  for (const FstabEntry& entry : entries) {
    CHECK(!EndsWith(entry.mount_point, '/'));
    std::string_view sub_dir = entry.mount_point;
    CHECK(ConsumePrefix(&sub_dir, source));
    if (sub_dir.empty()) {
      // `source` itself. Already mounted.
      continue;
    }
    std::string target_sub_dir = target;
    target_sub_dir += sub_dir;
    OR_RETURN(BindMount(entry.mount_point, target_sub_dir));
  }
  return {};
}

std::string GetBlockDeviceName(const std::string& partition, const std::string& slot) {
  return ART_FORMAT("{}/{}{}", kDeviceMapperDir, partition, slot);
}

Result<std::vector<std::string>> GetSupportedFilesystems() {
  std::string content;
  if (!ReadFileToString("/proc/filesystems", &content)) {
    return ErrnoErrorf("Failed to read '/proc/filesystems'");
  }
  std::vector<std::string> filesystems;
  for (const std::string& line : Split(content, "\n")) {
    std::vector<std::string> tokens = Tokenize(line, " \t");
    // If there are two tokens, the first token is a "nodev" mark, meaning it's not for a block
    // device, so we skip it.
    if (tokens.size() == 1) {
      filesystems.push_back(tokens[0]);
    }
  }
  return filesystems;
}

Result<void> Mount(const std::string& block_device, const std::string& target) {
  static const NoDestructor<Result<std::vector<std::string>>> supported_filesystems(
      GetSupportedFilesystems());
  if (!supported_filesystems->ok()) {
    return supported_filesystems->error();
  }
  std::vector<std::string> error_msgs;
  for (const std::string& filesystem : supported_filesystems->value()) {
    if (mount(block_device.c_str(),
              target.c_str(),
              filesystem.c_str(),
              MS_RDONLY,
              /*data=*/nullptr) == 0) {
      // Success.
      LOG(INFO) << ART_FORMAT(
          "Mounted '{}' at '{}' with type '{}'", block_device, target, filesystem);
      return {};
    } else {
      error_msgs.push_back(ART_FORMAT("Tried '{}': {}", filesystem, strerror(errno)));
      if (errno != EINVAL && errno != EBUSY) {
        // If the filesystem type is wrong, `errno` must be either `EINVAL` or `EBUSY`.
        break;
      }
    }
  }
  return Errorf("Failed to mount '{}' at '{}':\n{}", block_device, target, Join(error_msgs, '\n'));
}

Result<void> MountTmpfs(const std::string& target, std::string_view se_context) {
  if (mount(/*source=*/"tmpfs",
            target.c_str(),
            /*fs_type=*/"tmpfs",
            MS_NODEV | MS_NOEXEC | MS_NOSUID,
            ART_FORMAT("mode={:#o},rootcontext={}", kMode, se_context).c_str()) != 0) {
    return ErrnoErrorf("Failed to mount tmpfs at '{}'", target);
  }
  return {};
}

}  // namespace

ScopedAStatus DexoptChrootSetup::setUp(const std::optional<std::string>& in_otaSlot) {
  std::lock_guard<std::mutex> lock(mu_);
  if (in_otaSlot.has_value() && (in_otaSlot.value() != "_a" && in_otaSlot.value() != "_b")) {
    return Fatal(ART_FORMAT("Invalid OTA slot '{}'", in_otaSlot.value()));
  }
  OR_RETURN_NON_FATAL(SetUpChroot(in_otaSlot));
  return ScopedAStatus::ok();
}

ScopedAStatus DexoptChrootSetup::tearDown() {
  std::lock_guard<std::mutex> lock(mu_);
  OR_RETURN_NON_FATAL(TearDownChroot());
  return ScopedAStatus::ok();
}

Result<void> DexoptChrootSetup::Start() {
  ScopedAStatus status = ScopedAStatus::fromStatus(
      AServiceManager_registerLazyService(this->asBinder().get(), kServiceName));
  if (!status.isOk()) {
    return Error() << status.getDescription();
  }

  ABinderProcess_startThreadPool();

  return {};
}

Result<void> DexoptChrootSetup::SetUpChroot(const std::optional<std::string>& ota_slot) const {
  // Set the default permission mode for new files and dirs to be `kMode`.
  umask(~kMode & 0777);

  // In case there is some leftover.
  OR_RETURN(TearDownChroot());

  // Prepare the root dir of chroot.
  OR_RETURN(CreateDirs(CHROOT_DIR));
  LOG(INFO) << ART_FORMAT("Created '{}'", CHROOT_DIR);

  std::vector<std::string> additional_system_partitions = {
      "system_ext",
      "vendor",
      "product",
  };

  if (!ota_slot.has_value()) {
    OR_RETURN(BindMount("/", CHROOT_DIR));
    for (const std::string& partition : additional_system_partitions) {
      OR_RETURN(BindMount("/" + partition, CHROOT_DIR + ("/" + partition)));
    }
  } else {
    CHECK(ota_slot.value() != "_a" || ota_slot.value() != "_b");

    // Run `snapshotctl map` through init to map block devices. We can't run it ourselves because it
    // requires the UID to be 0.
    if (!SetProperty("sys.snapshotctl.map", "requested")) {
      return Errorf("Failed to request snapshotctl map");
    }
    if (!WaitForProperty("sys.snapshotctl.map", "finished", kSnapshotCtlTimeout)) {
      return Errorf("snapshotctl timed out");
    }

    // We don't know whether snapshotctl succeeded or not, but if it failed, the mount operation
    // below will fail with `ENOENT`.
    OR_RETURN(Mount(GetBlockDeviceName("system", ota_slot.value()), CHROOT_DIR));
    for (const std::string& partition : additional_system_partitions) {
      OR_RETURN(
          Mount(GetBlockDeviceName(partition, ota_slot.value()), CHROOT_DIR + ("/" + partition)));
    }
  }

  OR_RETURN(MountTmpfs(CHROOT_DIR + "/apex"s, "u:object_r:apex_mnt_dir:s0"));
  OR_RETURN(MountTmpfs(CHROOT_DIR + "/linkerconfig"s, "u:object_r:linkerconfig_file:s0"));
  OR_RETURN(MountTmpfs(CHROOT_DIR + "/mnt"s, "u:object_r:pre_reboot_dexopt_file:s0"));
  OR_RETURN(CreateDirs(CHROOT_DIR + "/mnt/artd_tmp"s));
  OR_RETURN(MountTmpfs(CHROOT_DIR + "/mnt/artd_tmp"s, "u:object_r:pre_reboot_dexopt_artd_file:s0"));
  OR_RETURN(CreateDirs(CHROOT_DIR + "/mnt/expand"s));

  std::vector<std::string> bind_mount_srcs = {
      // Data partitions.
      "/data",
      "/mnt/expand",
      // Linux API filesystems.
      "/dev",
      "/proc",
      "/sys",
      // For apexd to query staged APEX sessions.
      "/metadata",
  };

  for (const std::string& src : bind_mount_srcs) {
    OR_RETURN(BindMountRecursive(src, CHROOT_DIR + src));
  }

  // Generate empty linker config to suppress warnings.
  if (!android::base::WriteStringToFile("", CHROOT_DIR + "/linkerconfig/ld.config.txt"s)) {
    PLOG(WARNING) << "Failed to generate empty linker config to suppress warnings";
  }

  CmdlineBuilder args;
  args.Add(OR_RETURN(GetArtExec()))
      .Add("--chroot=%s", CHROOT_DIR)
      .Add("--")
      .Add("/system/bin/apexd")
      .Add("--otachroot-bootstrap")
      .AddIf(!ota_slot.has_value(), "--also-include-staged-apexes");
  OR_RETURN(Run("apexd", args.Get()));

  args = CmdlineBuilder();
  args.Add(OR_RETURN(GetArtExec()))
      .Add("--chroot=%s", CHROOT_DIR)
      .Add("--drop-capabilities")
      .Add("--")
      .Add("/apex/com.android.runtime/bin/linkerconfig")
      .Add("--target")
      .Add("/linkerconfig");
  OR_RETURN(Run("linkerconfig", args.Get()));

  return {};
}

Result<void> DexoptChrootSetup::TearDownChroot() const {
  if (OS::FileExists((CHROOT_DIR + "/system/bin/apexd"s).c_str())) {
    CmdlineBuilder args;
    args.Add(OR_RETURN(GetArtExec()))
        .Add("--chroot=%s", CHROOT_DIR)
        .Add("--")
        .Add("/system/bin/apexd")
        .Add("--unmount-all")
        .Add("--also-include-staged-apexes");
    if (Result<void> result = Run("apexd", args.Get()); !result.ok()) {
      // Maybe apexd is not executable because a previous setup/teardown failed halfway (e.g.,
      // /system is currently mounted but /dev is not). We do a check below to see if there is any
      // unmounted APEXes.
      LOG(WARNING) << "Failed to run apexd: " << result.error().message();
    }
  }

  std::vector<FstabEntry> apex_entries =
      OR_RETURN(GetProcMountsDescendantsOfPath(CHROOT_DIR + "/apex"s));
  for (const FstabEntry& entry : apex_entries) {
    if (entry.mount_point != CHROOT_DIR + "/apex"s) {
      return Errorf("apexd didn't unmount '{}'. See logs for details", entry.mount_point);
    }
  }

  // The list is in mount order.
  std::vector<FstabEntry> entries = OR_RETURN(GetProcMountsDescendantsOfPath(CHROOT_DIR));
  for (auto it = entries.rbegin(); it != entries.rend(); it++) {
    if (umount2(it->mount_point.c_str(), UMOUNT_NOFOLLOW) != 0) {
      return ErrnoErrorf("Failed to umount2 '{}'", it->mount_point);
    }
    LOG(INFO) << ART_FORMAT("Unmounted '{}'", it->mount_point);
  }

  std::error_code ec;
  std::uintmax_t removed = std::filesystem::remove_all(CHROOT_DIR, ec);
  if (ec) {
    return Errorf("Failed to remove dir '{}': {}", CHROOT_DIR, ec.message());
  }
  if (removed > 0) {
    LOG(INFO) << ART_FORMAT("Removed '{}'", CHROOT_DIR);
  }

  if (!OR_RETURN(GetProcMountsDescendantsOfPath(kBindMountTmpDir)).empty() &&
      umount2(kBindMountTmpDir, UMOUNT_NOFOLLOW) != 0) {
    return ErrnoErrorf("Failed to umount2 '{}'", kBindMountTmpDir);
  }

  std::filesystem::remove_all(kBindMountTmpDir, ec);
  if (ec) {
    return Errorf("Failed to remove dir '{}': {}", kBindMountTmpDir, ec.message());
  }

  if (!SetProperty("sys.snapshotctl.unmap", "requested")) {
    return Errorf("Failed to request snapshotctl unmap");
  }
  if (!WaitForProperty("sys.snapshotctl.unmap", "finished", kSnapshotCtlTimeout)) {
    return Errorf("snapshotctl timed out");
  }

  return {};
}

}  // namespace dexopt_chroot_setup
}  // namespace art
