/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_OPENJDKJVMTI_METHOD_DETOURS_H_
#define ART_RUNTIME_OPENJDKJVMTI_METHOD_DETOURS_H_

#include "jvmti.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DETOURS_VERSION_1_0 = 0x40010000,
    DETOURS_VERSION = DETOURS_VERSION_1_0
};

// Opaque detour IDs

struct _jdetourID;
typedef struct _detourID* jdetourID;

// Environment declarations

struct _DetoursEnv;
struct DetoursInterface;

#if defined(__cplusplus)
typedef _DetoursEnv detoursEnv;
#else
typedef const struct DetoursInterface* detoursEnv;
#endif

// Detour flags
enum {
    DETOURS_VIRTUAL_INTERCEPT = 0x0001,
};

// Plain C interface
struct DetoursInterface {
  jvmtiError (JNICALL *InstallMethodDetour) (detoursEnv* env,
    jmethodID target,
    jmethodID detour,
    jint flags,
    jdetourID* original);

  jvmtiError (JNICALL *RemoveMethodDetour) (detoursEnv* env,
    jdetourID detour);

  jvmtiError (*ToDetourObject)(detoursEnv* env,
    jdetourID detour_id,
    jobject* detour_ptr);

  jvmtiError (*FromDetourObject)(detoursEnv* env,
    jobject detour,
    jdetourID* detour_id_ptr);
};

// C++ interface wrapper
struct _DetoursEnv {
    const struct DetoursInterface* functions;

#if defined(__cplusplus)
  jvmtiError InstallMethodDetour(jmethodID target, jmethodID detour,
            jint flags, jdetourID* original) {
    return functions->InstallMethodDetour(this, target, detour, flags, original);
  }

  jvmtiError RemoveMethodDetour(jdetourID detour) {
    return functions->RemoveMethodDetour(this, detour);
  }

  jvmtiError ToDetourObject(jdetourID detour_id, jobject* detour_ptr) {
    return functions->ToDetourObject(this, detour_id, detour_ptr);
  }

  jvmtiError FromDetourObject(jobject detour, jdetourID* detour_id_ptr) {
    return functions->FromDetourObject(this, detour, detour_id_ptr);
  }
#endif // __cplusplus
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // ART_RUNTIME_OPENJDKJVMTI_METHOD_DETOURS_H_
