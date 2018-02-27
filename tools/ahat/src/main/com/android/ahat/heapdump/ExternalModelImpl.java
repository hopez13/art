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
 * Implements the various methods to compute external models.
 */
class ExternalModelImpl {

  private static void applyNativeAllocationRegistryModel(Iterable<AhatInstance> instances) {
    // Update registered native allocation size.
    for (AhatInstance cleaner : instances) {
      AhatInstance.RegisteredNativeAllocation nra = cleaner.asRegisteredNativeAllocation();
      if (nra != null) {
        nra.referent.addExternalModel(nra.size);
      }
    }
  }

  private static void applyActionableMemoryMetricModels(Iterable<AhatInstance> instances) {
    for (AhatInstance inst : instances) {
      // Bitmap: 4 * mWidth * mHeight if mNativePtr != 0
      if ("android.graphics.Bitmap".equals(inst.getClassName())) {
        // TODO: test and handle case where pixel format isn't 4 bytes per pixel.
        Value width = inst.getField("mWidth");
        Value height = inst.getField("mHeight");
        Value nptr = inst.getField("mNativePtr");
        if (width != null && height != null && nptr != null
            && width.isInteger() && height.isInteger() && nptr.isLong() && nptr.asLong() != 0) {
          inst.addExternalModel(4 * width.asInteger() * height.asInteger());
        }
      }

      // TextureView: 2 * 4 * (mRight - mLeft) * (mBottom - mTop)
      if ("android.view.TextureView".equals(inst.getClassName())) {
        Value right = inst.getField("mRight");
        Value left = inst.getField("mLeft");
        Value bottom = inst.getField("mBottom");
        Value top = inst.getField("mTop");
        if (right != null && left != null && bottom != null && top != null
            && right.isInteger() && left.isInteger() && bottom.isInteger() && top.isInteger()) {
          int width = right.asInteger() - left.asInteger();
          int height = bottom.asInteger() - top.asInteger();
          inst.addExternalModel(2 * (4 * width * height));
        }
      }

      // TODO: Add more models.
    }
  }

  static void applyExternalModel(ExternalModelSource source, Iterable<AhatInstance> instances) {
    switch (source) {
      case NATIVE_ALLOCATION_REGISTRY:
        applyNativeAllocationRegistryModel(instances);
        break;

      case ACTIONABLE_MEMORY_METRIC:
        applyActionableMemoryMetricModels(instances);
    }
  }
}
