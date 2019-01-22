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

import libcore.util.FP16;

public class Main {
  public Main() {
  }

  public static int TestFP16ToFloatRawIntBits(short half) {
      float f = FP16.toFloat(half);
      // Since in this test class we need to check the integer representing of
      // the actual float NaN values, the floatToRawIntBits() is used instead of
      // floatToIntBits().
      return Float.floatToRawIntBits(f);
  }

  public static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  public static void assertEquals(float expected, float actual) {
      if (expected != actual) {
          throw new Error("Expected: " + expected + ", found: " + actual);
      }
  }

  public static void main(String args[]) {
    // Test FP16 to float
    // Test some FP16 values
    assertEquals(0x3f800000, TestFP16ToFloatRawIntBits((short)0x3c00));  // 1.0f
    assertEquals(0x3f000000, TestFP16ToFloatRawIntBits((short)0x3800));  // 0.5f
    assertEquals(0x3e800000, TestFP16ToFloatRawIntBits((short)0x3400));  // 0.25f
    assertEquals(0x3e000000, TestFP16ToFloatRawIntBits((short)0x3000));  // 0.125f
    assertEquals(0x3d800000, TestFP16ToFloatRawIntBits((short)0x2c00));  // 0.0625f
    assertEquals(0x3d000000, TestFP16ToFloatRawIntBits((short)0x2800));  // 0.03125f

    assertEquals(0xbf800000, TestFP16ToFloatRawIntBits((short)0xbc00));  // -1.0f
    assertEquals(0xbf000000, TestFP16ToFloatRawIntBits((short)0xb800));  // -0.5f
    assertEquals(0xbe800000, TestFP16ToFloatRawIntBits((short)0xb400));  // -0.25f
    assertEquals(0xbe000000, TestFP16ToFloatRawIntBits((short)0xb000));  // -0.125f
    assertEquals(0xbd800000, TestFP16ToFloatRawIntBits((short)0xac00));  // -0.0625f
    assertEquals(0xbd000000, TestFP16ToFloatRawIntBits((short)0xa800));  // -0.03125f

    // Some more special FP16 values
    // 0 01111 0000000001 = 1 + 2^-10 = 1.0009765625 (next smallest float after 1)
    assertEquals(0x3f802000, TestFP16ToFloatRawIntBits((short)0x3c01));
    // 0 11110 1111111111 = 65504  (max half precision)
    assertEquals(0x477fe000, TestFP16ToFloatRawIntBits((short)0x7bff));
    // 0 00001 0000000000 = 2^-14 ~= 6.10352 × 10^-5 (minimum positive normal)
    assertEquals(0x38800000, TestFP16ToFloatRawIntBits((short)0x400));
    // 0 00000 1111111111 = 2^-14 - 2^-24 ~= 6.09756 × 10^-5 (maximum subnormal)
    assertEquals(0x387fc000, TestFP16ToFloatRawIntBits((short)0x3ff));
    // 0 00000 0000000001 = 2^-24 ~= 5.96046 × 10^-8 (minimum positive subnormal)
    assertEquals(0x33800000, TestFP16ToFloatRawIntBits((short)0x1));

    // FP16 +/- zero to float
    assertEquals(0x80000000, TestFP16ToFloatRawIntBits((short)0x8000));
    assertEquals(0x0, TestFP16ToFloatRawIntBits((short)0x0));

    // FP16 +/- infinity to float
    assertEquals(Float.NEGATIVE_INFINITY, FP16.toFloat((short)0xfc00));
    assertEquals(Float.POSITIVE_INFINITY, FP16.toFloat((short)0x7c00));

    // FP16 SNaN/QNaN inputs to float
    // The most significant bit of mantissa:
    //                 V
    // 0xfc01: 1 11111 0000000001 (signaling NaN)
    // 0xfdff: 1 11111 0111111111 (signaling NaN)
    // 0xfe00: 1 11111 1000000000 (quiet NaN)
    // 0xffff: 1 11111 1111111111 (quiet NaN)
    // This test is inspired by Java implementation of android.util.Half.toFloat(),
    // where the implementation performs SNaN->QNaN conversion.
    assert(Float.isNaN(FP16.toFloat((short)0xfc01)));
    assert(Float.isNaN(FP16.toFloat((short)0xfdff)));
    assert(Float.isNaN(FP16.toFloat((short)0xfe00)));
    assert(Float.isNaN(FP16.toFloat((short)0xffff)));
    assertEquals(0xffc02000, TestFP16ToFloatRawIntBits((short)(0xfc01)));  // SNaN->QNaN
    assertEquals(0xffffe000, TestFP16ToFloatRawIntBits((short)(0xfdff)));  // SNaN->QNaN
    assertEquals(0xffc00000, TestFP16ToFloatRawIntBits((short)(0xfe00)));  // QNaN->QNaN
    assertEquals(0xffffe000, TestFP16ToFloatRawIntBits((short)(0xffff)));  // QNaN->QNaN
  }
}
