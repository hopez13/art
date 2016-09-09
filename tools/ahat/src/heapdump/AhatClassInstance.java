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

package com.android.ahat.heapdump;

import com.android.tools.perflib.heap.ClassInstance;
import com.android.tools.perflib.heap.Instance;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class AhatClassInstance extends AhatInstance {
  private List<FieldValue> mFieldValues;
  private Map<String, Value> mUniqueFieldsByName;

  @Override void initialize(InitializeHelper helper, Instance inst) {
    ClassInstance classInst = (ClassInstance)inst;
    super.initialize(helper, classInst);

    mFieldValues = new ArrayList<FieldValue>();
    mUniqueFieldsByName = new HashMap<String, Value>();
    for (ClassInstance.FieldValue field : classInst.getValues()) {
      String name = field.getField().getName();
      String type = field.getField().getType().toString();
      Value value = helper.getValue(field.getValue());

      mFieldValues.add(new FieldValue(name, type, value));

      if (mUniqueFieldsByName.containsKey(name)) {
        // This is a duplicate field, meaning it isn't unique. Update the
        // mapping to 'null' to indicate there are duplicate fields with this
        // name.
        mUniqueFieldsByName.put(name, null);
      } else {
        mUniqueFieldsByName.put(name, value);
      }
    }
  }

  @Override public Value getField(String fieldName) {
    return mUniqueFieldsByName.get(fieldName);
  }

  /**
   * Read a reference field of this instance.
   * Returns null if the field value is null, or if the field couldn't be read.
   */
  public AhatInstance getRefField(String fieldName) {
    Value value = getField(fieldName);
    return value == null ? null : value.asAhatInstance();
  }

  /**
   * Returns the field values for this instance.
   */
  public List<FieldValue> getValues() {
    return mFieldValues;
  }

  /**
   * Returns true if the given instance is an instance of a class with the
   * given name.
   */
  private boolean isInstanceOfClass(String className) {
    AhatClassObj cls = getClassObj();
    while (cls != null) {
      if (className.equals(cls.getClassName())) {
        return true;
      }
      cls = cls.getSuperClassObj();
    }
    return false;
  }

  @Override public String asString(int maxChars) {
    if (!isInstanceOfClass("java.lang.String")) {
      return null;
    }

    Value value = getField("value");
    if (!value.isAhatInstance()) {
     return null;
    }
    
    AhatInstance inst = value.asAhatInstance();
    if (inst.isArrayInstance()) {
      return inst.asArrayInstance().asString(maxChars);
    }
    return null;
  }

  @Override public AhatInstance getReferent() {
    if (isInstanceOfClass("java.lang.ref.Reference")) {
      return getRefField("referent");
    }
    return null;
  }

  @Override public NativeAllocation getNativeAllocation() {
    if (!isInstanceOfClass("libcore.util.NativeAllocationRegistry$CleanerThunk")) {
      return null;
    }

    Long pointer = getLongField("nativePtr", null);
    if (pointer == null) {
      return null;
    }

    // Search for the registry field of inst.
    AhatInstance registry = null;
    for (FieldValue field : mFieldValues) {
      Value fieldValue = field.getValue();
      if (fieldValue.isAhatInstance()) {
        AhatClassInstance fieldInst = fieldValue.asAhatInstance().asClassInstance();
        if (fieldInst != null
            && fieldInst.isInstanceOfClass("libcore.util.NativeAllocationRegistry")) {
          registry = fieldInst;
          break;
        }
      }
    }

    if (registry == null) {
      return null;
    }

    Long size = registry.getLongField("size", null);
    if (size == null) {
      return null;
    }

    AhatInstance referent = null;
    for (AhatInstance ref : getHardReverseReferences()) {
      if (ref.isClassInstance() && ref.asClassInstance().isInstanceOfClass("sun.misc.Cleaner")) {
        referent = ref.getReferent();
        if (referent != null) {
          break;
        }
      }
    }

    if (referent == null) {
      return null;
    }
    return new NativeAllocation(size, getHeap(), pointer, referent);
  }

  @Override public String getDexCacheLocation(int maxChars) {
    if (isInstanceOfClass("java.lang.DexCache")) {
      AhatInstance location = getRefField("location");
      if (location != null) {
        return location.asString(maxChars);
      }
    }
    return null;
  }

  @Override public AhatInstance getAssociatedBitmapInstance() {
    if (isInstanceOfClass("android.graphics.Bitmap")) {
      return this;
    }
    return null;
  }

  @Override public boolean isClassInstance() {
    return true;
  }

  @Override public AhatClassInstance asClassInstance() {
    return this;
  }

  @Override public String toString() {
    String className = getClassObj().getClassName();
    return String.format("%s@%08x", className, getId());
  }

  /**
   * Read the given field from the given instance.
   * The field is assumed to be a byte[] field.
   * Returns null if the field value is null, not a byte[] or could not be read.
   */
  private byte[] getByteArrayField(String fieldName) {
    Value value = getField(fieldName);
    if (!value.isAhatInstance()) {
      return null;
    }
    return value.asAhatInstance().asByteArray();
  }

  public BufferedImage asBitmap() {
    if (!isInstanceOfClass("android.graphics.Bitmap")) {
      return null;
    }

    Integer width = getIntField("mWidth", null);
    if (width == null) {
      return null;
    }

    Integer height = getIntField("mHeight", null);
    if (height == null) {
      return null;
    }

    byte[] buffer = getByteArrayField("mBuffer");
    if (buffer == null) {
      return null;
    }

    // Convert the raw data to an image
    // Convert BGRA to ABGR
    int[] abgr = new int[height * width];
    for (int i = 0; i < abgr.length; i++) {
      abgr[i] = (
          (((int) buffer[i * 4 + 3] & 0xFF) << 24)
          + (((int) buffer[i * 4 + 0] & 0xFF) << 16)
          + (((int) buffer[i * 4 + 1] & 0xFF) << 8)
          + ((int) buffer[i * 4 + 2] & 0xFF));
    }

    BufferedImage bitmap = new BufferedImage(
        width, height, BufferedImage.TYPE_4BYTE_ABGR);
    bitmap.setRGB(0, 0, width, height, abgr, 0, width);
    return bitmap;
  }
}
