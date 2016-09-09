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

import com.android.tools.perflib.heap.ArrayInstance;
import com.android.tools.perflib.heap.Instance;
import com.android.tools.perflib.heap.Type;

import java.util.List;

public class AhatArrayInstance extends AhatInstance {
  private Value[] mValues = null;
  private boolean mIsCharArray;
  private boolean mIsByteArray;

  @Override void initialize(InitializeHelper helper, Instance inst) {
    ArrayInstance array = (ArrayInstance)inst;
    super.initialize(helper, array);

    Object[] objects = array.getValues();
    mValues = new Value[objects.length];
    for (int i = 0; i < objects.length; i++) {
      mValues[i] = helper.getValue(objects[i]);
    }

    mIsCharArray = (array.getArrayType() == Type.CHAR);
    mIsByteArray = (array.getArrayType() == Type.BYTE);
  }
  
  /**
   * Returns the array's values.
   */
  public Value[] getValues() {
    return mValues;
  }

  /**
   * Return the object at the given index of this array.
   */
  public Value getValue(int index) {
    return mValues[index];
  }

  @Override public boolean isArrayInstance() {
    return true;
  }

  @Override public AhatArrayInstance asArrayInstance() {
    return this;
  }

  @Override public String asString(int maxChars) {
    if (!mIsCharArray) {
      return null;
    }

    int numChars = mValues.length;
    int count = getIntField("count", numChars);
    if (count == 0) {
      return "";
    }
    if (0 <= maxChars && maxChars < count) {
      count = maxChars;
    }

    int offset = getIntField("offset", 0);
    int end = offset + count - 1;
    if (offset >= 0 && offset < numChars && end >= 0 && end < numChars) {
      char[] data = new char[count];
      for (int i = 0; i < count; i++) {
        data[i] = mValues[i + offset].asChar();
      }
      return new String(data);
    }
    return null;
  }

  @Override public AhatInstance getAssociatedBitmapInstance() {
    if (mIsByteArray) {
      List<AhatInstance> refs = getHardReverseReferences();
      if (refs.size() == 1) {
        AhatInstance ref = refs.get(0);
        return ref.getAssociatedBitmapInstance();
      }
    }
    return null;
  }

  @Override public String toString() {
    String className = getClassObj().getClassName();
    if (className.endsWith("[]")) {
      className = className.substring(0, className.length() - 2);
    }
    return String.format("%s[%d]@%08x", className, mValues.length, getId());
  }

  byte[] asByteArray() {
    if (mIsByteArray) {
      byte[] bytes = new byte[mValues.length];
      for (int i = 0; i < mValues.length; i++) {
        bytes[i] = mValues[i].asByte().byteValue();
      }
      return bytes;
    }
    return null;
  }
}
