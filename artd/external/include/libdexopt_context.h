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

#ifndef ART_ARTD_EXTERNAL_INCLUDE_LIBDEXOPT_CONTEXT_H_
#define ART_ARTD_EXTERNAL_INCLUDE_LIBDEXOPT_CONTEXT_H_

#include <stdint.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * Opaque type that contains the dexopt execution context.
 *
 * Introduced in API 33.
 */
typedef struct ADexoptContext ADexoptContext;

/**
 * Create a dexopt execution context given a marshaled byte array from ART. Return null if a valid
 * context cannot be created.
 *
 * @param marshaled a pointer to a byte array marshaled by an ART component, and can be unmarshaled
 *                  in this function.
 * @param size size of the byte array
 * @return an opaque object that contains the dexopt context, or null if invalid.
 *
 * Available since API level 33.
 */
const ADexoptContext* ADexopt_CreateAndValidateDexoptContext(const uint8_t* marshaled, size_t size)
        __INTRODUCED_IN(33);

/**
 * Delete the dexopt execution context.
 *
 * @param context an opaque object returned by ADexopt_CreateAndValidateDexoptContext.
 *
 * Available since API level 33.
 */
void ADexopt_DeleteDexoptContext(const ADexoptContext* context) __INTRODUCED_IN(33);

/**
 * Returns the command line arguments (excluding the executable path) in the execution context.
 * The client must provide a valid, living context, otherwise the behavior is undefined.
 *
 * @param context An opaque object returned by ADexopt_CreateAndValidateDexoptContext.
 * @return an C string array terminated by null.
 *
 * Available since API level 33.
 */
const char* const* ADexopt_GetCmdlineArguments(const ADexoptContext* context) __INTRODUCED_IN(33);

__END_DECLS

#endif  // ART_ARTD_EXTERNAL_INCLUDE_LIBDEXOPT_CONTEXT_H_
