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

#pragma once

#include <stdlib.h>
#include <string.h>
#include <sys/capability.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

uint8_t* DecodeBase64(const char* src, size_t* dst_size);
bool WriteBase64ToFile(const char* base64, const std::string& file,
        uid_t uid, gid_t gid, int mode, std::string* error_msg);
