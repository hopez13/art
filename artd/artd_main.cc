/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
#include <stdlib.h>

#include "android-base/logging.h"
#include "android-base/macros.h"
#include "android/binder_interface_utils.h"
#include "android/binder_process.h"
#include "artd.h"

int main([[maybe_unused]] int argc, char* argv[]) {
  android::base::InitLogging(argv);

  auto artd = ndk::SharedRefBase::make<art::artd::Artd>();

  LOG(INFO) << "Starting artd";

  if (auto ret = artd->Start(); !ret.ok()) {
    LOG(ERROR) << "Unable to start artd: " << ret.error();
    exit(1);
  }

  ABinderProcess_joinThreadPool();

  LOG(INFO) << "artd shutting down";

  return 0;
}
