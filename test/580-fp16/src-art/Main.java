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

    // This is a copy of libcore.util.FP16.toFloat() pure Java implementation.
    private static final int FP16_SIGN_SHIFT        = 15;
    private static final int FP16_SIGN_MASK         = 0x8000;
    private static final int FP16_EXPONENT_SHIFT    = 10;
    private static final int FP16_EXPONENT_MASK     = 0x1f;
    private static final int FP16_SIGNIFICAND_MASK  = 0x3ff;
    private static final int FP16_EXPONENT_BIAS     = 15;
    private static final int FP16_COMBINED          = 0x7fff;
    private static final int FP16_EXPONENT_MAX      = 0x7c00;
    private static final int FP32_SIGN_SHIFT        = 31;
    private static final int FP32_EXPONENT_SHIFT    = 23;
    private static final int FP32_EXPONENT_MASK     = 0xff;
    private static final int FP32_SIGNIFICAND_MASK  = 0x7fffff;
    private static final int FP32_EXPONENT_BIAS     = 127;
    private static final int FP32_QNAN_MASK         = 0x400000;
    private static final int FP32_DENORMAL_MAGIC = 126 << 23;
    private static final float FP32_DENORMAL_FLOAT = Float.intBitsToFloat(FP32_DENORMAL_MAGIC);
    public static float java_toFloat(short h) {
      int bits = h & 0xffff;
      int s = bits & FP16_SIGN_MASK;
      int e = (bits >>> FP16_EXPONENT_SHIFT) & FP16_EXPONENT_MASK;
      int m = (bits            ) & FP16_SIGNIFICAND_MASK;
      int outE = 0;
      int outM = 0;

      if (e == 0) { // Denormal or 0
        if (m != 0) {
          // Convert denorm fp16 into normalized fp32
          float o = Float.intBitsToFloat(FP32_DENORMAL_MAGIC + m);
          o -= FP32_DENORMAL_FLOAT;
          return s == 0 ? o : -o;
        }
      } else {
        outM = m << 13;
        if (e == 0x1f) { // Infinite or NaN
          outE = 0xff;
          if (outM != 0) { // SNaNs are quieted
            outM |= FP32_QNAN_MASK;
          }
        } else {
          outE = e - FP16_EXPONENT_BIAS + FP32_EXPONENT_BIAS;
        }
      }
      int out = (s << 16) | (outE << FP32_EXPONENT_SHIFT) | outM;
      return Float.intBitsToFloat(out);
    }
    public static float g_ret;
    public static void test_perf () {
      g_ret = 0.0f;
      long before = 0;
      long after = 0;

      // Time libcore FP16 implementation (intrinsic on ARM64)
      before = System.currentTimeMillis();
      for (int i = 0; i < 1000; i++) {
        for (short h = Short.MIN_VALUE; h < Short.MAX_VALUE; h++) {
          g_ret += FP16.toFloat(h);
        }
      }
      after = System.currentTimeMillis();
      System.out.println("Time of FP16.toFloat (ms): " + (after - before));

      // Time Java implementation
      before = System.currentTimeMillis();
      for (int i = 0; i < 1000; i++) {
        for (short h = Short.MIN_VALUE; h < Short.MAX_VALUE; h++) {
          g_ret += java_toFloat(h);
        }
      }
      after = System.currentTimeMillis();
      System.out.println("Time of Java version toFloat (ms): " + (after - before));
    }

    public static void main(String args[]) {
      // Measure FP16 performance.
      test_perf();

      // Test FP16 to float
      for (short h = Short.MIN_VALUE; h < Short.MAX_VALUE; h++) {
        if (FP16.isNaN(h)) {
          // NaN inputs are tested below.
          continue;
        }
        assertEquals(FP16.toHalf(FP16.toFloat(h)), h);
      }
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
