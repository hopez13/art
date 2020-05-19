/*
 * Copyright (C) 2020 The Android Open Source Project
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

public class RemTest {
  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main() {
    remInt();
    remLong();
  }

  private static void remInt() {
    expectEquals(0, $noinline$IntRemBy18(0));
    expectEquals(1, $noinline$IntRemBy18(1));
    expectEquals(-1, $noinline$IntRemBy18(-1));
    expectEquals(0, $noinline$IntRemBy18(18));
    expectEquals(0, $noinline$IntRemBy18(-18));
    expectEquals(11, $noinline$IntRemBy18(65));
    expectEquals(-11, $noinline$IntRemBy18(-65));

    expectEquals(0, $noinline$IntRemByMinus18(0));
    expectEquals(1, $noinline$IntRemByMinus18(1));
    expectEquals(-1, $noinline$IntRemByMinus18(-1));
    expectEquals(0, $noinline$IntRemByMinus18(18));
    expectEquals(0, $noinline$IntRemByMinus18(-18));
    expectEquals(11, $noinline$IntRemByMinus18(65));
    expectEquals(-11, $noinline$IntRemByMinus18(-65));

    expectEquals(0, $noinline$IntRemBy7(0));
    expectEquals(1, $noinline$IntRemBy7(1));
    expectEquals(-1, $noinline$IntRemBy7(-1));
    expectEquals(0, $noinline$IntRemBy7(7));
    expectEquals(0, $noinline$IntRemBy7(-7));
    expectEquals(1, $noinline$IntRemBy7(22));
    expectEquals(-1, $noinline$IntRemBy7(-22));

    expectEquals(0, $noinline$IntRemByMinus7(0));
    expectEquals(1, $noinline$IntRemByMinus7(1));
    expectEquals(-1, $noinline$IntRemByMinus7(-1));
    expectEquals(0, $noinline$IntRemByMinus7(7));
    expectEquals(0, $noinline$IntRemByMinus7(-7));
    expectEquals(1, $noinline$IntRemByMinus7(22));
    expectEquals(-1, $noinline$IntRemByMinus7(-22));

    expectEquals(0, $noinline$IntRemBy6(0));
    expectEquals(1, $noinline$IntRemBy6(1));
    expectEquals(-1, $noinline$IntRemBy6(-1));
    expectEquals(0, $noinline$IntRemBy6(6));
    expectEquals(0, $noinline$IntRemBy6(-6));
    expectEquals(1, $noinline$IntRemBy6(19));
    expectEquals(-1, $noinline$IntRemBy6(-19));

    expectEquals(0, $noinline$IntRemByMinus6(0));
    expectEquals(1, $noinline$IntRemByMinus6(1));
    expectEquals(-1, $noinline$IntRemByMinus6(-1));
    expectEquals(0, $noinline$IntRemByMinus6(6));
    expectEquals(0, $noinline$IntRemByMinus6(-6));
    expectEquals(1, $noinline$IntRemByMinus6(19));
    expectEquals(-1, $noinline$IntRemByMinus6(-19));

    expectEquals(0, $noinline$IntRemBy17(0));
    expectEquals(1, $noinline$IntRemBy17(1));
    expectEquals(-1, $noinline$IntRemBy17(-1));
    expectEquals(0, $noinline$IntRemBy17(17));
    expectEquals(0, $noinline$IntRemBy17(-17));
    expectEquals(1, $noinline$IntRemBy17(18));
    expectEquals(-1, $noinline$IntRemBy17(-18));

    expectEquals(0, $noinline$IntRemByMinus17(0));
    expectEquals(1, $noinline$IntRemByMinus17(1));
    expectEquals(-1, $noinline$IntRemByMinus17(-1));
    expectEquals(0, $noinline$IntRemByMinus17(17));
    expectEquals(0, $noinline$IntRemByMinus17(-17));
    expectEquals(1, $noinline$IntRemByMinus17(18));
    expectEquals(-1, $noinline$IntRemByMinus17(-18));

    expectEquals(0, $noinline$IntRemBy20(0));
    expectEquals(1, $noinline$IntRemBy20(1));
    expectEquals(-1, $noinline$IntRemBy20(-1));
    expectEquals(0, $noinline$IntRemBy20(20));
    expectEquals(0, $noinline$IntRemBy20(-20));
    expectEquals(1, $noinline$IntRemBy20(21));
    expectEquals(-1, $noinline$IntRemBy20(-21));

    expectEquals(0, $noinline$IntRemByMinus20(0));
    expectEquals(1, $noinline$IntRemByMinus20(1));
    expectEquals(-1, $noinline$IntRemByMinus20(-1));
    expectEquals(0, $noinline$IntRemByMinus20(20));
    expectEquals(0, $noinline$IntRemByMinus20(-20));
    expectEquals(1, $noinline$IntRemByMinus20(21));
    expectEquals(-1, $noinline$IntRemByMinus20(-21));

    expectEquals(0, $noinline$IntRemBy28(0));
    expectEquals(1, $noinline$IntRemBy28(1));
    expectEquals(-1, $noinline$IntRemBy28(-1));
    expectEquals(0, $noinline$IntRemBy28(28));
    expectEquals(0, $noinline$IntRemBy28(-28));
    expectEquals(1, $noinline$IntRemBy28(29));
    expectEquals(-1, $noinline$IntRemBy28(-29));

    expectEquals(0, $noinline$IntRemByMinus28(0));
    expectEquals(1, $noinline$IntRemByMinus28(1));
    expectEquals(-1, $noinline$IntRemByMinus28(-1));
    expectEquals(0, $noinline$IntRemByMinus28(28));
    expectEquals(0, $noinline$IntRemByMinus28(-28));
    expectEquals(1, $noinline$IntRemByMinus28(29));
    expectEquals(-1, $noinline$IntRemByMinus28(-29));

    expectEquals(0, $noinline$IntRemByMaxInt(0));
    expectEquals(1, $noinline$IntRemByMaxInt(1));
    expectEquals(-1, $noinline$IntRemByMaxInt(-1));
    expectEquals(0, $noinline$IntRemByMaxInt(Integer.MAX_VALUE));
    expectEquals(0, $noinline$IntRemByMaxInt(Integer.MIN_VALUE + 1));

    expectEquals(0, $noinline$IntRemByMinIntPlus1(0));
    expectEquals(1, $noinline$IntRemByMinIntPlus1(1));
    expectEquals(-1, $noinline$IntRemByMinIntPlus1(-1));
    expectEquals(0, $noinline$IntRemByMinIntPlus1(Integer.MAX_VALUE));
    expectEquals(0, $noinline$IntRemByMinIntPlus1(Integer.MIN_VALUE + 1));

    expectEquals(0, $noinline$IntRemBy100(0));
    expectEquals(1, $noinline$IntRemBy100(1));
    expectEquals(-1, $noinline$IntRemBy100(-1));
    expectEquals(0, $noinline$IntRemBy100(100));
    expectEquals(0, $noinline$IntRemBy100(-100));
    expectEquals(1, $noinline$IntRemBy100(101));
    expectEquals(-1, $noinline$IntRemBy100(-101));

    expectEquals(0, $noinline$IntRemByMinus100(0));
    expectEquals(1, $noinline$IntRemByMinus100(1));
    expectEquals(-1, $noinline$IntRemByMinus100(-1));
    expectEquals(0, $noinline$IntRemByMinus100(100));
    expectEquals(0, $noinline$IntRemByMinus100(-100));
    expectEquals(1, $noinline$IntRemByMinus100(101));
    expectEquals(-1, $noinline$IntRemByMinus100(-101));
  }

  // A test case to check:
  //  1. 'lsr' and 'asr' are combined into one 'asr'. For divisor 18 seen in an
  //  MP3 decoding workload there is no need to correct the result of
  //  get_high(dividend * magic). So there are no instructions between 'lsr' and
  //  'asr'. In such a case they can be combined into one 'asr'.
  //  2. 'msub' strength reduction. Divisor 18 is 2^4 + 2. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy18(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #3
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #1
  private static int $noinline$IntRemBy18(int v) {
    int r = v % 18;
    return r;
  }

  // As 'n % -18' == 'n % 18', the test case checks the generated code is the same
  // as for 'n % 18'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus18(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #3
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #1
  private static int $noinline$IntRemByMinus18(int v) {
    int r = v % -18;
    return r;
  }

  // A test case to check:
  //  1. 'lsr' and 'add' are combined into one 'adds'. For divisor 7 seen in the core
  //  library the result of get_high(dividend * magic) must be corrected by the 'add' instruction.
  //  2. 'add' and 'add_shift' are optimized into 'adds' and 'cinc'.
  //  3. 'msub' strength reduction. Divisor 7 is 2^3 - 1. So the remainder can be
  //  found by using 'sub' and 'add'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy7(int) disassembly (after)
  /// CHECK:                 adds x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #32
  /// CHECK-NEXT:            asr  x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            cinc w{{\d+}}, w{{\d+}}, mi
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #3
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemBy7(int v) {
    int r = v % 7;
    return r;
  }

  // As 'n % -7' == 'n % 7', the test case checks the generated code is the same
  // as for 'n % 7'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus7(int) disassembly (after)
  /// CHECK:                 adds x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #32
  /// CHECK-NEXT:            asr  x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            cinc w{{\d+}}, w{{\d+}}, mi
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #3
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemByMinus7(int v) {
    int r = v % -7;
    return r;
  }

  // A test case to check:
  //  1. 'asr' is used to get the high 32 bits of the result of 'dividend * magic'.
  //  For divisor 6 seen in the core library there is no need to correct the result of
  //  get_high(dividend * magic). Also there is no 'asr' before the final 'add' instruction
  //  which uses only the high 32 bits of the result. In such a case 'asr' getting the high
  //  32 bits can be used as well.
  //  2. 'msub' strength reduction. Divisor 6 is 2^2 + 2. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy6(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #1
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #1
  private static int $noinline$IntRemBy6(int v) {
    int r = v % 6;
    return r;
  }

  // As 'n % -6' == 'n % 6', the test case checks the generated code is the same
  // as for 'n % 6'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus6(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #1
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #1
  private static int $noinline$IntRemByMinus6(int v) {
    int r = v % -6;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 17 is 2^4 + 1. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy17(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #35
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #4
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemBy17(int v) {
    int r = v % 17;
    return r;
  }

  // As 'n % -17' == 'n % 17', the test case checks the generated code is the same
  // as for 'n % 17'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus17(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #35
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #4
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemByMinus17(int v) {
    int r = v % -17;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 20 is 2^4 + 4. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy20(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #35
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #2
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #2
  private static int $noinline$IntRemBy20(int v) {
    int r = v % 20;
    return r;
  }

  // As 'n % -20' == 'n % 20', the test case checks the generated code is the same
  // as for 'n % 20'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus20(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #35
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #2
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #2
  private static int $noinline$IntRemByMinus20(int v) {
    int r = v % -20;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 28 is 2^5 - 4. So the remainder can be
  //  found by using 'sub' and 'add'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy28(int) disassembly (after)
  /// CHECK:                 cinc w16, w16, mi
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #3
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #2
  private static int $noinline$IntRemBy28(int v) {
    int r = v % 28;
    return r;
  }

  // As 'n % -28' == 'n % 28', the test case checks the generated code is the same
  // as for 'n % 28'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus28(int) disassembly (after)
  /// CHECK:                 cinc w16, w16, mi
  /// CHECK-NEXT:            sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #3
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #2
  private static int $noinline$IntRemByMinus28(int v) {
    int r = v % -28;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor Integer.MAX_VALUE is 2^31 - 1. So the remainder can be
  //  found by using 'sub' and 'add'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMaxInt(int) disassembly (after)
  /// CHECK:                 sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemByMaxInt(int v) {
    int r = v % Integer.MAX_VALUE;
    return r;
  }

  // As 'n % (Integer.MIN_VALUE + 1)' == 'n % Integer.MAX_VALUE', the test case checks the
  // generated code is the same as for 'n % Integer.MAX_VALUE'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinIntPlus1(int) disassembly (after)
  /// CHECK:                 sub w{{\d+}}, w{{\d+}}, w{{\d+}}, lsl #31
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemByMinIntPlus1(int v) {
    int r = v % (Integer.MIN_VALUE + 1);
    return r;
  }

  // A test case to check that 'msub' strength reduction cannot be applied to divisor 100.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemBy100(int) disassembly (after)
  /// CHECK:                 mov w{{\d+}}, #0x64
  /// CHECK-NEXT:            msub w{{\d+}}, w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemBy100(int v) {
    int r = v % 100;
    return r;
  }

  // As 'n % -100' == 'n % 100', the test case checks the generated code is the same
  // as for 'n % 100'.
  //
  /// CHECK-START-ARM64: int RemTest.$noinline$IntRemByMinus100(int) disassembly (after)
  /// CHECK:                 mov w{{\d+}}, #0x64
  /// CHECK-NEXT:            msub w{{\d+}}, w{{\d+}}, w{{\d+}}, w{{\d+}}
  private static int $noinline$IntRemByMinus100(int v) {
    int r = v % -100;
    return r;
  }

  private static void remLong() {
    expectEquals(0L, $noinline$LongRemBy18(0L));
    expectEquals(1L, $noinline$LongRemBy18(1L));
    expectEquals(-1L, $noinline$LongRemBy18(-1L));
    expectEquals(0L, $noinline$LongRemBy18(18L));
    expectEquals(0L, $noinline$LongRemBy18(-18L));
    expectEquals(11L, $noinline$LongRemBy18(65L));
    expectEquals(-11L, $noinline$LongRemBy18(-65L));

    expectEquals(0L, $noinline$LongRemByMinus18(0L));
    expectEquals(1L, $noinline$LongRemByMinus18(1L));
    expectEquals(-1L, $noinline$LongRemByMinus18(-1L));
    expectEquals(0L, $noinline$LongRemByMinus18(18L));
    expectEquals(0L, $noinline$LongRemByMinus18(-18L));
    expectEquals(11L, $noinline$LongRemByMinus18(65L));
    expectEquals(-11L, $noinline$LongRemByMinus18(-65L));

    expectEquals(0L, $noinline$LongRemBy7(0L));
    expectEquals(1L, $noinline$LongRemBy7(1L));
    expectEquals(-1L, $noinline$LongRemBy7(-1L));
    expectEquals(0L, $noinline$LongRemBy7(7L));
    expectEquals(0L, $noinline$LongRemBy7(-7L));
    expectEquals(1L, $noinline$LongRemBy7(22L));
    expectEquals(-1L, $noinline$LongRemBy7(-22L));

    expectEquals(0L, $noinline$LongRemByMinus7(0L));
    expectEquals(1L, $noinline$LongRemByMinus7(1L));
    expectEquals(-1L, $noinline$LongRemByMinus7(-1L));
    expectEquals(0L, $noinline$LongRemByMinus7(7L));
    expectEquals(0L, $noinline$LongRemByMinus7(-7L));
    expectEquals(1L, $noinline$LongRemByMinus7(22L));
    expectEquals(-1L, $noinline$LongRemByMinus7(-22L));

    expectEquals(0L, $noinline$LongRemBy6(0L));
    expectEquals(1L, $noinline$LongRemBy6(1L));
    expectEquals(-1L, $noinline$LongRemBy6(-1L));
    expectEquals(0L, $noinline$LongRemBy6(6L));
    expectEquals(0L, $noinline$LongRemBy6(-6L));
    expectEquals(1L, $noinline$LongRemBy6(19L));
    expectEquals(-1L, $noinline$LongRemBy6(-19L));

    expectEquals(0L, $noinline$LongRemByMinus6(0L));
    expectEquals(1L, $noinline$LongRemByMinus6(1L));
    expectEquals(-1L, $noinline$LongRemByMinus6(-1L));
    expectEquals(0L, $noinline$LongRemByMinus6(6L));
    expectEquals(0L, $noinline$LongRemByMinus6(-6L));
    expectEquals(1L, $noinline$LongRemByMinus6(19L));
    expectEquals(-1L, $noinline$LongRemByMinus6(-19L));

    expectEquals(0L, $noinline$LongRemBy100(0L));
    expectEquals(1L, $noinline$LongRemBy100(1L));
    expectEquals(-1L, $noinline$LongRemBy100(-1L));
    expectEquals(0L, $noinline$LongRemBy100(100L));
    expectEquals(0L, $noinline$LongRemBy100(-100L));
    expectEquals(1L, $noinline$LongRemBy100(101L));
    expectEquals(-1L, $noinline$LongRemBy100(-101L));

    expectEquals(0L, $noinline$LongRemByMinus100(0L));
    expectEquals(1L, $noinline$LongRemByMinus100(1L));
    expectEquals(-1L, $noinline$LongRemByMinus100(-1L));
    expectEquals(0L, $noinline$LongRemByMinus100(100L));
    expectEquals(0L, $noinline$LongRemByMinus100(-100L));
    expectEquals(1L, $noinline$LongRemByMinus100(101L));
    expectEquals(-1L, $noinline$LongRemByMinus100(-101L));

    expectEquals(0L, $noinline$LongRemBy17(0L));
    expectEquals(1L, $noinline$LongRemBy17(1L));
    expectEquals(-1L, $noinline$LongRemBy17(-1L));
    expectEquals(0L, $noinline$LongRemBy17(17L));
    expectEquals(0L, $noinline$LongRemBy17(-17L));
    expectEquals(1L, $noinline$LongRemBy17(18L));
    expectEquals(-1L, $noinline$LongRemBy17(-18L));

    expectEquals(0L, $noinline$LongRemByMinus17(0L));
    expectEquals(1L, $noinline$LongRemByMinus17(1L));
    expectEquals(-1L, $noinline$LongRemByMinus17(-1L));
    expectEquals(0L, $noinline$LongRemByMinus17(17L));
    expectEquals(0L, $noinline$LongRemByMinus17(-17L));
    expectEquals(1L, $noinline$LongRemByMinus17(18L));
    expectEquals(-1L, $noinline$LongRemByMinus17(-18L));

    expectEquals(0L, $noinline$LongRemBy20(0L));
    expectEquals(1L, $noinline$LongRemBy20(1L));
    expectEquals(-1L, $noinline$LongRemBy20(-1L));
    expectEquals(0L, $noinline$LongRemBy20(20L));
    expectEquals(0L, $noinline$LongRemBy20(-20L));
    expectEquals(1L, $noinline$LongRemBy20(21L));
    expectEquals(-1L, $noinline$LongRemBy20(-21L));

    expectEquals(0L, $noinline$LongRemByMinus20(0L));
    expectEquals(1L, $noinline$LongRemByMinus20(1L));
    expectEquals(-1L, $noinline$LongRemByMinus20(-1L));
    expectEquals(0L, $noinline$LongRemByMinus20(20L));
    expectEquals(0L, $noinline$LongRemByMinus20(-20L));
    expectEquals(1L, $noinline$LongRemByMinus20(21L));
    expectEquals(-1L, $noinline$LongRemByMinus20(-21L));

    expectEquals(0L, $noinline$LongRemBy28(0L));
    expectEquals(1L, $noinline$LongRemBy28(1L));
    expectEquals(-1L, $noinline$LongRemBy28(-1L));
    expectEquals(0L, $noinline$LongRemBy28(28L));
    expectEquals(0L, $noinline$LongRemBy28(-28L));
    expectEquals(1L, $noinline$LongRemBy28(29L));
    expectEquals(-1L, $noinline$LongRemBy28(-29L));

    expectEquals(0L, $noinline$LongRemByMinus28(0L));
    expectEquals(1L, $noinline$LongRemByMinus28(1L));
    expectEquals(-1L, $noinline$LongRemByMinus28(-1L));
    expectEquals(0L, $noinline$LongRemByMinus28(28L));
    expectEquals(0L, $noinline$LongRemByMinus28(-28L));
    expectEquals(1L, $noinline$LongRemByMinus28(29L));
    expectEquals(-1L, $noinline$LongRemByMinus28(-29L));

    expectEquals(0L, $noinline$LongRemByMaxLong(0L));
    expectEquals(1L, $noinline$LongRemByMaxLong(1L));
    expectEquals(-1L, $noinline$LongRemByMaxLong(-1L));
    expectEquals(0L, $noinline$LongRemByMaxLong(Long.MAX_VALUE));
    expectEquals(0L, $noinline$LongRemByMaxLong(Long.MIN_VALUE + 1));

    expectEquals(0L, $noinline$LongRemByMinLongPlus1(0L));
    expectEquals(1L, $noinline$LongRemByMinLongPlus1(1L));
    expectEquals(-1L, $noinline$LongRemByMinLongPlus1(-1L));
    expectEquals(0L, $noinline$LongRemByMinLongPlus1(Long.MAX_VALUE));
    expectEquals(0L, $noinline$LongRemByMinLongPlus1(Long.MIN_VALUE + 1));
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 18 is 2^4 + 2. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy18(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #3
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #1
  private static long $noinline$LongRemBy18(long v) {
    long r = v % 18L;
    return r;
  }

  // As 'n % -18' == 'n % 18', the test case checks the generated code is the same
  // as for 'n % 18'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus18(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #3
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #1
  private static long $noinline$LongRemByMinus18(long v) {
    long r = v % -18L;
    return r;
  }


  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 7 is 2^3 - 1. So the remainder can be
  //  found by using 'sub' and 'add'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy7(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr x{{\d+}}, x{{\d+}}, #1
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #3
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemBy7(long v) {
    long r = v % 7L;
    return r;
  }

  // As 'n % -7' == 'n % 7', the test case checks the generated code is the same
  // as for 'n % 7'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus7(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr x{{\d+}}, x{{\d+}}, #1
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #3
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemByMinus7(long v) {
    long r = v % -7L;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 6 is 2^2 + 2. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy6(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #1
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #1
  private static long $noinline$LongRemBy6(long v) {
    long r = v % 6L;
    return r;
  }

  // As 'n % -6' == 'n % 6', the test case checks the generated code is the same
  // as for 'n % 6'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus6(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #1
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #1
  private static long $noinline$LongRemByMinus6(long v) {
    long r = v % -6L;
    return r;
  }

  // A test to check 'add' and 'add_shift' are optimized into 'adds' and 'cinc'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy100(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            adds  x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr   x{{\d+}}, x{{\d+}}, #6
  /// CHECK-NEXT:            cinc  x{{\d+}}, x{{\d+}}, mi
  /// CHECK-NEXT:            mov x{{\d+}}, #0x64
  /// CHECK-NEXT:            msub x{{\d+}}, x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemBy100(long v) {
    long r = v % 100L;
    return r;
  }

  // A test to check 'sub' and 'add_shift' are optimized into 'subs' and 'cinc'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus100(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            adds  x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr   x{{\d+}}, x{{\d+}}, #6
  /// CHECK-NEXT:            cinc  x{{\d+}}, x{{\d+}}, mi
  /// CHECK-NEXT:            mov x{{\d+}}, #0x64
  /// CHECK-NEXT:            msub x{{\d+}}, x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemByMinus100(long v) {
    long r = v % -100L;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 17 is 2^4 + 1. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy17(long) disassembly (after)
  /// CHECK:                 add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #4
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemBy17(long v) {
    long r = v % 17L;
    return r;
  }

  // As 'n % -17' == 'n % 17', the test case checks the generated code is the same
  // as for 'n % 17'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus17(long) disassembly (after)
  /// CHECK:                 add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #4
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemByMinus17(long v) {
    long r = v % -17L;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 20 is 2^4 + 4. So the remainder can be
  //  found by using 'add' and 'sub'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy20(long) disassembly (after)
  /// CHECK:                 add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #2
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #2
  private static long $noinline$LongRemBy20(long v) {
    long r = v % 20L;
    return r;
  }

  // As 'n % -20' == 'n % 20', the test case checks the generated code is the same
  // as for 'n % 20'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus20(long) disassembly (after)
  /// CHECK:                 add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #2
  /// CHECK-NEXT:            sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #2
  private static long $noinline$LongRemByMinus20(long v) {
    long r = v % -20L;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor 28 is 2^5 - 4. So the remainder can be
  //  found by using 'sub' and 'add'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemBy28(long) disassembly (after)
  /// CHECK:                 sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #3
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #2
  private static long $noinline$LongRemBy28(long v) {
    long r = v % 28L;
    return r;
  }

  // As 'n % -28' == 'n % 28', the test case checks the generated code is the same
  // as for 'n % 28'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinus28(long) disassembly (after)
  /// CHECK:                 sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #3
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #2
  private static long $noinline$LongRemByMinus28(long v) {
    long r = v % -28L;
    return r;
  }

  // A test case to check:
  //  1. 'msub' strength reduction. Divisor Long.MAX_VALUE is 2^63 - 1. So the remainder can be
  //  found by using 'sub' and 'add'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMaxLong(long) disassembly (after)
  /// CHECK:                 sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #63
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemByMaxLong(long v) {
    long r = v % Long.MAX_VALUE;
    return r;
  }

  // As 'n % (Long.MIN_VALUE + 1)' == 'n % Long.MAX_VALUE', the test case checks the
  // generated code is the same as for 'n % Long.MAX_VALUE'.
  //
  /// CHECK-START-ARM64: long RemTest.$noinline$LongRemByMinLongPlus1(long) disassembly (after)
  /// CHECK:                 sub x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #63
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}
  private static long $noinline$LongRemByMinLongPlus1(long v) {
    long r = v % (Long.MIN_VALUE + 1);
    return r;
  }
}
