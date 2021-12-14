/*
 * Copyright (C) 2022 The Android Open Source Project
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

/**
 * Tests for autovectorization of loops with control flow.
 */
public class Main {

  public static final int kArrayLength = 128;
  public static final int kUsedArrayLength = kArrayLength - 1;

  public static boolean[] booleanArray = new boolean[kArrayLength];
  public static boolean[] booleanArray2 = new boolean[kArrayLength];
  public static byte[] byteArray = new byte[kArrayLength];
  public static short[] shortArray = new short[kArrayLength];
  public static char[] charArray = new char[kArrayLength];
  public static int[] intArray = new int[kArrayLength];
  public static long[] longArray = new long[kArrayLength];
  public static float[] floatArray = new float[kArrayLength];
  public static double[] doubleArray = new double[kArrayLength];

  public static final int kMagicValueA = 2;
  public static final int kMagicValueB = 10;
  public static final int kMagicValueC = 100;

  public static final int kMagicAddConst = 99;

  public static final float kMagicFloatValueA = 2.0f;
  public static final float kMagicFloatValueB = 10.0f;
  public static final float kMagicFloatValueC = 100.0f;

  public static final float kMagicFloatAddConst = 99.0f;

  /// CHECK-START-ARM64: int Main.$compile$noinline$FullDiamond(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: <<C0:i\d+>>      IntConstant 0                                         loop:none
  ///     CHECK-DAG: <<C4:i\d+>>      IntConstant 4                                         loop:none
  ///     CHECK-DAG: <<C99:i\d+>>     IntConstant 99                                        loop:none
  ///     CHECK-DAG: <<C100:i\d+>>    IntConstant 100                                       loop:none
  ///     CHECK-DAG: <<Vec4:d\d+>>    VecReplicateScalar [<<C4>>,{{j\d+}}]                  loop:none
  ///     CHECK-DAG: <<Vec99:d\d+>>   VecReplicateScalar [<<C99>>,{{j\d+}}]                 loop:none
  ///     CHECK-DAG: <<Vec100:d\d+>>  VecReplicateScalar [<<C100>>,{{j\d+}}]                loop:none
  //
  ///     CHECK-DAG: <<Phi:i\d+>>     Phi [<<C0>>,{{i\d+}}]                                 loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK-DAG: <<LoopP:j\d+>>   VecPredWhile [<<Phi>>,{{i\d+}}]                       loop:<<Loop>>      outer_loop:none
  //
  ///     CHECK-DAG: <<Load1:d\d+>>   VecLoad [<<Arr:l\d+>>,<<Phi>>,<<LoopP>>]              loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<Cond:j\d+>>    VecCondition [<<Load1>>,<<Vec100>>,<<LoopP>>]         loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<CondR:j\d+>>   VecPredNot [<<Cond>>,<<LoopP>>]                       loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<AddT:d\d+>>    VecAdd [<<Load1>>,<<Vec99>>,<<CondR>>]                loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<StT:d\d+>>     VecStore [<<Arr>>,<<Phi>>,<<AddT>>,<<CondR>>]         loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<StF:d\d+>>     VecStore [<<Arr>>,<<Phi>>,{{d\d+}},<<Cond>>]          loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<Ld2:d\d+>>     VecLoad [<<Arr>>,<<Phi>>,<<LoopP>>]                   loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<Add2:d\d+>>    VecAdd [<<Ld2>>,<<Vec4>>,<<LoopP>>]                   loop:<<Loop>>      outer_loop:none
  ///     CHECK-DAG: <<St21:d\d+>>    VecStore [<<Arr>>,<<Phi>>,<<Add2>>,<<LoopP>>]         loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-ELSE:
  //
  ///     CHECK-NOT:                      VecLoad
  //
  /// CHECK-FI:
  public static int $compile$noinline$FullDiamond(int[] x) {
    int i = 0;
    for (; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      } else {
        x[i] += 3;
      }
      x[i] += 4;
    }
    return i;
  }

  //
  // Test various types.
  //

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleBoolean(boolean[], boolean[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: Support extra condition types and boolean comparisons.
  public static void $compile$noinline$SimpleBoolean(boolean[] x, boolean[] y) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      boolean val = x[i];
      if (val != y[i]) {
        x[i] |= y[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleByte(byte[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$SimpleByte(byte[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      byte val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleUByte(byte[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$SimpleUByte(byte[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      if ((x[i] & 0xFF) != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleShort(short[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$SimpleShort(short[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      short val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleChar(char[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$SimpleChar(char[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      char val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleInt(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$SimpleInt(int[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleLong(long[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: Support long comparisons.
  public static void $compile$noinline$SimpleLong(long[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      long val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleFloat(float[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: Support FP comparisons.
  public static void $compile$noinline$SimpleFloat(float[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      float val = x[i];
      if (val > 10.0f) {
        x[i] += 99.1f;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleDouble(double[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: Support FP comparisons.
  public static void $compile$noinline$SimpleDouble(double[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      double val = x[i];
      if (val != 10.0) {
        x[i] += 99.1;
      }
    }
  }

  //
  // Narrowing types.
  //

  /// CHECK-START-ARM64: void Main.$compile$noinline$ByteConv(byte[], byte[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$ByteConv(byte[] x, byte[] y) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      byte val = (byte)(x[i] + 1);
      if (val != y[i]) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$UByteAndWrongConst(byte[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // 'NarrowerOperands' not met: the constant is not a ubyte one.
  public static void $compile$noinline$UByteAndWrongConst(byte[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      if ((x[i] & 0xFF) != (kMagicValueC | 0x100)) {
        x[i] += kMagicAddConst;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$ByteNoHiBits(byte[], byte[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // Check kNoHiBits case when "wider" operations cannot bring in higher order bits.
  public static void $compile$noinline$ByteNoHiBits(byte[] x, byte[] y) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      byte val = x[i];
      if ((val >>> 3) != y[i]) {
        x[i] += kMagicAddConst;
      }
    }
  }

  //
  // Test condition types.
  //

  /// CHECK-START-ARM64: void Main.$compile$noinline$SimpleBelow(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: Support other conditions.
  public static void $compile$noinline$SimpleBelow(int[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val < kMagicValueC) {
        x[i] += kMagicAddConst;
      }
    }
  }

  //
  // Test vectorization idioms.
  //

  /// CHECK-START-ARM64: void Main.$compile$noinline$Select(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: vectorize loops with select in the body.
  public static void $compile$noinline$Select(int[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        val += kMagicAddConst;
      }
      x[i] = val;
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$Phi(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: vectorize loops with phis in the body.
  public static void $compile$noinline$Phi(int[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        val += kMagicAddConst;
        x[i] += val;
      }
      x[i] += val;
    }
  }

  // TODO: when Phis are supported, test dotprod and sad idioms.

  /// CHECK-START-ARM64: int Main.$compile$noinline$Reduction(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // TODO: vectorize loops with phis and reductions in the body.
  private static int $compile$noinline$Reduction(int[] x) {
    int sum = 0;
    for (int i = 0; i < kArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        sum += val + x[i];
      }
    }
    return sum;
  }

  /// CHECK-START-ARM64: int Main.$compile$noinline$ReductionBackEdge(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-DAG: VecLoad
  //
  /// CHECK-FI:
  //
  // Reduction in the back edge block, non-CF-dependent.
  public static int $compile$noinline$ReductionBackEdge(int[] x) {
    int sum = 0;
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
      }
      sum += x[i];
    }
    return sum;
  }

  //
  // Negative compile tests.
  //

  public static final int kStencilArraySize = 130;

  /// CHECK-START-ARM64: void Main.$compile$noinline$stencilAlike(int[], int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // This loop needs a runtime test for array references disambiguation and a scalar cleanup loop.
  // Currently we can't generate a scalar clean up loop with control flow.
  private static void $compile$noinline$stencilAlike(int[] a, int[] b) {
    for (int i = 1; i < kStencilArraySize - 1; i++) {
      int val0 = b[i - 1];
      int val1 = b[i];
      int val2 = b[i + 1];
      int un = a[i];
      if (val1 != kMagicValueC) {
        a[i] = val0 + val1 + val2;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$NotDiamondCf(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  //
  // Loops with complex CF are not supported.
  public static void $compile$noinline$NotDiamondCf(int[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        if (val != 1234) {
          x[i] += kMagicAddConst;
        }
      }
    }
  }

  /// CHECK-START-ARM64: void Main.$compile$noinline$BrokenInduction(int[]) loop_optimization (after)
  /// CHECK-IF:     hasIsaFeature("sve")
  //
  ///     CHECK-NOT: VecLoad
  //
  /// CHECK-FI:
  public static void $compile$noinline$BrokenInduction(int[] x) {
    for (int i = 0; i < kUsedArrayLength; i++) {
      int val = x[i];
      if (val != kMagicValueC) {
        x[i] += kMagicAddConst;
        i++;
      }
    }
  }

  //
  // Main driver.
  //

  public static void main(String[] args) {
    initIntArray(intArray);
    int final_ind_value = $compile$noinline$FullDiamond(intArray);
    expectIntEquals(23755, IntArraySum(intArray));
    expectIntEquals(kUsedArrayLength, final_ind_value);

    // Types.
    initBooleanArray(booleanArray);
    booleanArray2[12] = true;
    $compile$noinline$SimpleBoolean(booleanArray, booleanArray2);
    expectIntEquals(86, BooleanArraySum(booleanArray));

    initByteArray(byteArray);
    $compile$noinline$SimpleByte(byteArray);
    expectIntEquals(-64, ByteArraySum(byteArray));

    initByteArray(byteArray);
    $compile$noinline$SimpleUByte(byteArray);
    expectIntEquals(-64, ByteArraySum(byteArray));

    initShortArray(shortArray);
    $compile$noinline$SimpleShort(shortArray);
    expectIntEquals(23121, ShortArraySum(shortArray));

    initCharArray(charArray);
    $compile$noinline$SimpleChar(charArray);
    expectIntEquals(23121, CharArraySum(charArray));

    initIntArray(intArray);
    $compile$noinline$SimpleInt(intArray);
    expectIntEquals(23121, IntArraySum(intArray));

    initLongArray(longArray);
    $compile$noinline$SimpleLong(longArray);
    expectLongEquals(23121, LongArraySum(longArray));

    initFloatArray(floatArray);
    $compile$noinline$SimpleFloat(floatArray);
    expectFloatEquals(18868.2f, FloatArraySum(floatArray));

    initDoubleArray(doubleArray);
    $compile$noinline$SimpleDouble(doubleArray);
    expectDoubleEquals(23129.5, DoubleArraySum(doubleArray));

    // Narrowing types.
    initByteArray(byteArray);
    $compile$noinline$ByteConv(byteArray, byteArray);
    expectIntEquals(-2, ByteArraySum(byteArray));

    initByteArray(byteArray);
    $compile$noinline$UByteAndWrongConst(byteArray);
    expectIntEquals(-2, ByteArraySum(byteArray));

    initByteArray(byteArray);
    $compile$noinline$ByteNoHiBits(byteArray, byteArray);
    expectIntEquals(-2, ByteArraySum(byteArray));

    // Conditions.
    initIntArray(intArray);
    $compile$noinline$SimpleBelow(intArray);
    expectIntEquals(23121, IntArraySum(intArray));

    // Idioms.
    initIntArray(intArray);
    $compile$noinline$Select(intArray);
    expectIntEquals(23121, IntArraySum(intArray));

    initIntArray(intArray);
    $compile$noinline$Phi(intArray);
    expectIntEquals(36748, IntArraySum(intArray));

    int reduction_result = 0;

    initIntArray(intArray);
    reduction_result = $compile$noinline$Reduction(intArray);
    expectIntEquals(14706, IntArraySum(intArray));
    expectIntEquals(21012, reduction_result);

    initIntArray(intArray);
    reduction_result = $compile$noinline$ReductionBackEdge(intArray);
    expectIntEquals(23121, IntArraySum(intArray));
    expectIntEquals(13121, reduction_result);

    int[] stencilArrayA = new int[kStencilArraySize];
    int[] stencilArrayB = new int[kStencilArraySize];
    initIntArray(stencilArrayA);
    initIntArray(stencilArrayB);
    $compile$noinline$stencilAlike(stencilArrayA, stencilArrayB);
    expectIntEquals(43602, IntArraySum(stencilArrayA));

    initIntArray(intArray);
    $compile$noinline$NotDiamondCf(intArray);
    expectIntEquals(23121, IntArraySum(intArray));

    initIntArray(intArray);
    $compile$noinline$BrokenInduction(intArray);
    expectIntEquals(18963, IntArraySum(intArray));

    System.out.println("passed");
  }

  public static void initBooleanArray(boolean[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 != 0) {
        a[i] = true;
      }
    }
  }

  public static void initByteArray(byte[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = (byte)kMagicValueA;
      } else if (i % 3 == 1) {
        a[i] = (byte)kMagicValueB;
      } else {
        a[i] = (byte)kMagicValueC;
      }
    }
    a[kUsedArrayLength] = 127;
  }

  public static void initShortArray(short[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = (short)kMagicValueA;
      } else if (i % 3 == 1) {
        a[i] = (short)kMagicValueB;
      } else {
        a[i] = (short)kMagicValueC;
      }
    }
    a[kUsedArrayLength] = 10000;
  }

  public static void initCharArray(char[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = (char)kMagicValueA;
      } else if (i % 3 == 1) {
        a[i] = (char)kMagicValueB;
      } else {
        a[i] = (char)kMagicValueC;
      }
    }
    a[kUsedArrayLength] = 10000;
  }

  public static void initIntArray(int[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = kMagicValueA;
      } else if (i % 3 == 1) {
        a[i] = kMagicValueB;
      } else {
        a[i] = kMagicValueC;
      }
    }
    a[kUsedArrayLength] = 10000;
  }

  public static void initLongArray(long[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = kMagicValueA;
      } else if (i % 3 == 1) {
        a[i] = kMagicValueB;
      } else {
        a[i] = kMagicValueC;
      }
    }
    a[kUsedArrayLength] = 10000;
  }

  public static void initFloatArray(float[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = kMagicFloatValueA;
      } else if (i % 3 == 1) {
        a[i] = kMagicFloatValueB;
      } else {
        a[i] = kMagicFloatValueC;
      }
    }
    a[kUsedArrayLength] = 10000.0f;
  }

  public static void initDoubleArray(double[] a) {
    for (int i = 0; i < kArrayLength; i++) {
      if (i % 3 == 0) {
        a[i] = kMagicFloatValueA;
      } else if (i % 3 == 1) {
        a[i] = kMagicFloatValueB;
      } else {
        a[i] = kMagicFloatValueC;
      }
    }
    a[kUsedArrayLength] = 10000.0f;
  }

  public static byte BooleanArraySum(boolean[] a) {
    byte sum = 0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i] ? 1 : 0;
    }
    return sum;
  }

  public static byte ByteArraySum(byte[] a) {
    byte sum = 0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  public static short ShortArraySum(short[] a) {
    short sum = 0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  public static char CharArraySum(char[] a) {
    char sum = 0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  public static int IntArraySum(int[] a) {
    int sum = 0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  public static long LongArraySum(long[] a) {
    long sum = 0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  public static float FloatArraySum(float[] a) {
    float sum = 0.0f;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  public static double DoubleArraySum(double[] a) {
    double sum = 0.0;
    for (int i = 0; i < a.length; i++) {
      sum += a[i];
    }
    return sum;
  }

  private static void expectIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectFloatEquals(float expected, float result) {
    final float THRESHOLD = .1f;
    if (Math.abs(expected - result) >= THRESHOLD) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectDoubleEquals(double expected, double result) {
    final double THRESHOLD = .1;
    if (Math.abs(expected - result) >= THRESHOLD) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
