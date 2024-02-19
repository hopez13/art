/*
 * Copyright (C) 2024 The Android Open Source Project
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

public class Main {
  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /// CHECK-START: int Main.$noinline$intRiscvShift1Add(int, int) instruction_simplifier_riscv64 (before)
  /// CHECK:          <<A:i\d+>>        ParameterValue
  /// CHECK:          <<B:i\d+>>        ParameterValue
  /// CHECK:          <<One:i\d+>>      IntConstant 1
  /// CHECK:          <<Shift:i\d+>>    Shl [<<A>>,<<One>>]
  /// CHECK:          <<Add:i\d+>>      Add [<<B>>,<<Shift>>]
  /// CHECK:          <<Return:v\d+>>   Return [<<Add>>]

  /// CHECK-START: int Main.$noinline$intRiscvShift1Add(int, int) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Riscv64Shift1Add

  public static int $noinline$intRiscvShift1Add(int a, int b) {
    return (a << 1) + b;
  }

  /// CHECK-START: long Main.$noinline$longIntRiscvShift1Add(long, int) instruction_simplifier_riscv64 (before)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:i\d+>>        ParameterValue
  /// CHECK:          <<One:i\d+>>      IntConstant 1
  /// CHECK:          <<Shift:j\d+>>    Shl [<<A>>,<<One>>]
  /// CHECK:          <<Convert:j\d+>>  TypeConversion [<<B>>]
  /// CHECK:          <<Add:j\d+>>      Add [<<Shift>>,<<Convert>>]
  /// CHECK:          <<Return:v\d+>>   Return [<<Add>>]

  /// CHECK-START: long Main.$noinline$longIntRiscvShift1Add(long, int) instruction_simplifier_riscv64 (after)
  /// CHECK:          <<ShiftAdd:j\d+>> Riscv64Shift1Add

  /// CHECK-START: long Main.$noinline$longIntRiscvShift1Add(long, int) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Shl

  /// CHECK-START: long Main.$noinline$longIntRiscvShift1Add(long, int) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Add

  public static long $noinline$longIntRiscvShift1Add(long a, int b) {
    return (a << 1) + b;
  }

  /// CHECK-START: long Main.$noinline$longRiscvShift1Add(long, long) instruction_simplifier_riscv64 (before)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<One:i\d+>>      IntConstant 1
  /// CHECK:          <<Shift:j\d+>>    Shl [<<A>>,<<One>>]
  /// CHECK:          <<Add:j\d+>>      Add [<<B>>,<<Shift>>]
  /// CHECK:          <<Return:v\d+>>   Return [<<Add>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift1Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<ShiftAdd:j\d+>> Riscv64Shift1Add [<<A>>,<<B>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift1Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Shl

  /// CHECK-START: long Main.$noinline$longRiscvShift1Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Add

  public static long $noinline$longRiscvShift1Add(long a, long b) {
    return (a << 1) + b;
  }

  /// CHECK-START: long Main.$noinline$longRiscvShift2Add(long, long) instruction_simplifier_riscv64 (before)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<Two:i\d+>>      IntConstant 2
  /// CHECK:          <<Shift:j\d+>>    Shl [<<A>>,<<Two>>]
  /// CHECK:          <<Add:j\d+>>      Add [<<B>>,<<Shift>>]
  /// CHECK:          <<Return:v\d+>>   Return [<<Add>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift2Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<ShiftAdd:j\d+>> Riscv64Shift2Add [<<A>>,<<B>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift2Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Shl

  /// CHECK-START: long Main.$noinline$longRiscvShift2Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Add

  public static long $noinline$longRiscvShift2Add(long a, long b) {
    return (a << 2) + b;
  }

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (before)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<Three:i\d+>>    IntConstant 3
  /// CHECK:          <<Shift:j\d+>>    Shl [<<A>>,<<Three>>]
  /// CHECK:          <<Add:j\d+>>      Add [<<B>>,<<Shift>>]
  /// CHECK:          <<Return:v\d+>>   Return [<<Add>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<ShiftAdd:j\d+>> Riscv64Shift3Add [<<A>>,<<B>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Shl

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Add

  public static long $noinline$longRiscvShift3Add(long a, long b) {
    return (a << 3) + b;
  }

  /// CHECK-START: long Main.$noinline$longReverseRiscvShift3Add(long, long) instruction_simplifier_riscv64 (before)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<Three:i\d+>>    IntConstant 3
  /// CHECK:          <<Shift:j\d+>>    Shl [<<A>>,<<Three>>]
  /// CHECK:          <<Add:j\d+>>      Add [<<B>>,<<Shift>>]
  /// CHECK:          <<Return:v\d+>>   Return [<<Add>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK:          <<A:j\d+>>        ParameterValue
  /// CHECK:          <<B:j\d+>>        ParameterValue
  /// CHECK:          <<ShiftAdd:j\d+>> Riscv64Shift3Add [<<A>>,<<B>>]

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Shl

  /// CHECK-START: long Main.$noinline$longRiscvShift3Add(long, long) instruction_simplifier_riscv64 (after)
  /// CHECK-NOT: Add

  public static long $noinline$longReverseRiscvShift3Add(long a, long b) {
    return b + (a << 3);
  }

  public static void main(String[] args) {
    assertIntEquals(0, $noinline$intRiscvShift1Add(0, 0));

    assertLongEquals(-3L, $noinline$longIntRiscvShift1Add(-1L, -1L));
    assertLongEquals(3L, $noinline$longIntRiscvShift1Add(1L, 1L));

    assertLongEquals(-3L, $noinline$longRiscvShift1Add(-1L, -1L));
    assertLongEquals(-1L, $noinline$longRiscvShift1Add(0L, -1L));
    assertLongEquals(-2L, $noinline$longRiscvShift1Add(-1L, 0L));
    assertLongEquals(0L, $noinline$longRiscvShift1Add(0L, 0L));
    assertLongEquals(1L, $noinline$longRiscvShift1Add(0L, 1L));
    assertLongEquals(2L, $noinline$longRiscvShift1Add(1L, 0L));
    assertLongEquals(3L, $noinline$longRiscvShift1Add(1L, 1L));

    assertLongEquals(-1L, $noinline$longRiscvShift1Add(Long.MAX_VALUE, 1L));

    assertLongEquals(0L, $noinline$longRiscvShift2Add(0L, 0L));
    assertLongEquals(1L, $noinline$longRiscvShift2Add(0L, 1L));
    assertLongEquals(4L, $noinline$longRiscvShift2Add(1L, 0L));
    assertLongEquals(5L, $noinline$longRiscvShift2Add(1L, 1L));

    assertLongEquals(0L, $noinline$longRiscvShift3Add(0L, 0L));
    assertLongEquals(0L, $noinline$longReverseRiscvShift3Add(0L, 0L));
    assertLongEquals(1L, $noinline$longRiscvShift3Add(0L, 1L));
    assertLongEquals(1L, $noinline$longReverseRiscvShift3Add(0L, 1L));
    assertLongEquals(8L, $noinline$longRiscvShift3Add(1L, 0L));
    assertLongEquals(8L, $noinline$longReverseRiscvShift3Add(1L, 0L));
    assertLongEquals(9L, $noinline$longRiscvShift3Add(1L, 1L));
    assertLongEquals(9L, $noinline$longReverseRiscvShift3Add(1L, 1L));
  }
}
