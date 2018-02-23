/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.ahat.heapdump;

/**
 * Enum describing the method used to compute external models.
 */
public enum ExternalModelSource {
  /**
   * The external size of an object is modeled as its registered
   * NativeAllocationRegistry size if it is registered.
   */
  NATIVE_ALLOCATION_REGISTRY,

  /**
   * The external size of an object is modeled based on the actionable memory
   * metric.
   */
  ACTIONABLE_MEMORY_METRIC;

  static ExternalModelSource getDefaultSource() {
    return NATIVE_ALLOCATION_REGISTRY;
  }
}
