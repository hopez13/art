/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBJNIART_INCLUDE_JNIART_LIBJNIART_API_H_
#define ART_LIBJNIART_INCLUDE_JNIART_LIBJNIART_API_H_

#include <stddef.h>

#include "jni.h"

// This is the stable C API exported by libjniart.

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/* ---------------------------------- C API for JniInvocation.h --------------------------------- */

/*
 * The JNI invocation API exists to allow a choice of library responsible for managing virtual
 * machines.
 */

/*
 * Opaque structure used to hold JNI invocation internal state.
 */
struct JniInvocationImpl;

/*
 * Creates an instance of a JniInvocationImpl.
 */
struct JniInvocationImpl* JniInvocationCreate();

/*
 * Associates a library with a JniInvocationImpl instance. The library should export C symbols for
 * JNI_GetDefaultJavaVMInitArgs, JNI_CreateJavaVM and JNI_GetDefaultJavaVMInitArgs.
 *
 * The specified |library| should be the filename of a shared library. The |library| is opened with
 * dlopen(3).
 *
 * If there is an error opening the specified |library|, then function will fallback to the
 * default library "libart.so". If the fallback library is successfully used then a warning is
 * written to the Android log buffer. Use of the fallback library is not considered an error.
 *
 * If the fallback library cannot be opened or the expected symbols are not found in the library
 * opened, then an error message is written to the Android log buffer and the function returns 0.
 *
 * Returns 1 on success, 0 otherwise.
 */
int JniInvocationInit(struct JniInvocationImpl* instance, const char* library);

/*
 * Release resources associated with JniInvocationImpl instance.
 */
void JniInvocationDestroy(struct JniInvocationImpl* instance);

/*
 * Gets the default library for JNI invocation. The default library is "libart.so". This value may
 * be overridden for debuggable builds using the persist.sys.dalvik.vm.lib.2 system property.
 *
 * The |library| argument is the preferred library to use on debuggable builds (when
 * ro.debuggable=1). If the |library| argument is nullptr, then the system preferred value will be
 * queried from persist.sys.dalvik.vm.lib.2 if the caller has provided |buffer| argument.
 *
 * The |buffer| argument is used for reading system properties in debuggable builds. It is
 * optional, but should be provisioned to be PROP_VALUE_MAX bytes if provided to ensure it is
 * large enough to hold a system property.
 *
 * Returns the filename of the invocation library determined from the inputs and system
 * properties. The returned value may be |library|, |buffer|, or a pointer to a string constant
 * "libart.so".
 */
const char* JniInvocationGetLibrary(const char* library, char* buffer);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // ART_LIBJNIART_INCLUDE_JNIART_LIBJNIART_API_H_
