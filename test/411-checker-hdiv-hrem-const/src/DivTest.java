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

public class DivTest {
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
    divInt();
    divLong();
  }

  private static void divInt() {
    expectEquals(0, $noinline$IntDivBy18(0));
    expectEquals(0, $noinline$IntDivBy18(1));
    expectEquals(0, $noinline$IntDivBy18(-1));
    expectEquals(1, $noinline$IntDivBy18(18));
    expectEquals(-1, $noinline$IntDivBy18(-18));
    expectEquals(3, $noinline$IntDivBy18(65));
    expectEquals(-3, $noinline$IntDivBy18(-65));

    expectEquals(0, $noinline$IntALenDivBy18(new int[0]));
    expectEquals(0, $noinline$IntALenDivBy18(new int[1]));
    expectEquals(1, $noinline$IntALenDivBy18(new int[18]));
    expectEquals(3, $noinline$IntALenDivBy18(new int[65]));

    expectEquals(0, $noinline$IntDivByMinus18(0));
    expectEquals(0, $noinline$IntDivByMinus18(1));
    expectEquals(0, $noinline$IntDivByMinus18(-1));
    expectEquals(-1, $noinline$IntDivByMinus18(18));
    expectEquals(1, $noinline$IntDivByMinus18(-18));
    expectEquals(-3, $noinline$IntDivByMinus18(65));
    expectEquals(3, $noinline$IntDivByMinus18(-65));

    expectEquals(0, $noinline$IntDivBy7(0));
    expectEquals(0, $noinline$IntDivBy7(1));
    expectEquals(0, $noinline$IntDivBy7(-1));
    expectEquals(1, $noinline$IntDivBy7(7));
    expectEquals(-1, $noinline$IntDivBy7(-7));
    expectEquals(3, $noinline$IntDivBy7(22));
    expectEquals(-3, $noinline$IntDivBy7(-22));

    expectEquals(0, $noinline$IntALenDivBy7(new int[0]));
    expectEquals(0, $noinline$IntALenDivBy7(new int[1]));
    expectEquals(1, $noinline$IntALenDivBy7(new int[7]));
    expectEquals(3, $noinline$IntALenDivBy7(new int[22]));

    expectEquals(0, $noinline$IntDivByMinus7(0));
    expectEquals(0, $noinline$IntDivByMinus7(1));
    expectEquals(0, $noinline$IntDivByMinus7(-1));
    expectEquals(-1, $noinline$IntDivByMinus7(7));
    expectEquals(1, $noinline$IntDivByMinus7(-7));
    expectEquals(-3, $noinline$IntDivByMinus7(22));
    expectEquals(3, $noinline$IntDivByMinus7(-22));

    expectEquals(0, $noinline$IntDivBy6(0));
    expectEquals(0, $noinline$IntDivBy6(1));
    expectEquals(0, $noinline$IntDivBy6(-1));
    expectEquals(1, $noinline$IntDivBy6(6));
    expectEquals(-1, $noinline$IntDivBy6(-6));
    expectEquals(3, $noinline$IntDivBy6(19));
    expectEquals(-3, $noinline$IntDivBy6(-19));

    expectEquals(0, $noinline$IntALenDivBy6(new int[0]));
    expectEquals(0, $noinline$IntALenDivBy6(new int[1]));
    expectEquals(1, $noinline$IntALenDivBy6(new int[6]));
    expectEquals(3, $noinline$IntALenDivBy6(new int[19]));

    expectEquals(0, $noinline$IntDivByMinus6(0));
    expectEquals(0, $noinline$IntDivByMinus6(1));
    expectEquals(0, $noinline$IntDivByMinus6(-1));
    expectEquals(-1, $noinline$IntDivByMinus6(6));
    expectEquals(1, $noinline$IntDivByMinus6(-6));
    expectEquals(-3, $noinline$IntDivByMinus6(19));
    expectEquals(3, $noinline$IntDivByMinus6(-19));

    expectEquals(0, $noinline$IntIndVarDivBy6(1));
    expectEquals(0, $noinline$IntIndVarDivBy6(6));
    expectEquals(5, $noinline$IntIndVarDivBy6(11));
  }

  // A test case to check that 'lsr' and 'asr' are combined into one 'asr'.
  // For divisor 18 seen in an MP3 decoding workload there is no need
  // to correct the result of get_high(dividend * magic). So there are no
  // instructions between 'lsr' and 'asr'. In such a case they can be combined
  // into one 'asr'.
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntDivBy18(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntDivBy18(int v) {
    int r = v / 18;
    return r;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.$noinline$IntALenDivBy18(int[]) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            lsr{{s?}} r{{\d+}}, r{{\d+}}, #2
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntALenDivBy18(int[]) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntALenDivBy18(int[] arr) {
    int r = arr.length / 18;
    return r;
  }

  // A test case to check that 'lsr' and 'asr' are combined into one 'asr'.
  // Divisor -18 has the same property as divisor 18: no need to correct the
  // result of get_high(dividend * magic). So there are no
  // instructions between 'lsr' and 'asr'. In such a case they can be combined
  // into one 'asr'.
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntDivByMinus18(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntDivByMinus18(int v) {
    int r = v / -18;
    return r;
  }

  // A test case to check that 'lsr' and 'add' are combined into one 'adds'.
  // For divisor 7 seen in the core library the result of get_high(dividend * magic)
  // must be corrected by the 'add' instruction.
  //
  // The test case also checks 'add' and 'add_shift' are optimized into 'adds' and 'cinc'.
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntDivBy7(int) disassembly (after)
  /// CHECK:                 adds x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #32
  /// CHECK-NEXT:            asr  x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            cinc w{{\d+}}, w{{\d+}}, mi
  private static int $noinline$IntDivBy7(int v) {
    int r = v / 7;
    return r;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.$noinline$IntALenDivBy7(int[]) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            add{{s?}} r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            lsr{{s?}} r{{\d+}}, r{{\d+}}, #2
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntALenDivBy7(int[]) disassembly (after)
  /// CHECK:                 adds x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #32
  /// CHECK-NEXT:            lsr  x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NOT:             cinc w{{\d+}}, w{{\d+}}, mi
  private static int $noinline$IntALenDivBy7(int[] arr) {
    int r = arr.length / 7;
    return r;
  }

  // A test case to check that 'lsr' and 'add' are combined into one 'adds'.
  // Divisor -7 has the same property as divisor 7: the result of get_high(dividend * magic)
  // must be corrected. In this case it is a 'sub' instruction.
  //
  // The test case also checks 'sub' and 'add_shift' are optimized into 'subs' and 'cinc'.
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntDivByMinus7(int) disassembly (after)
  /// CHECK:                 subs x{{\d+}}, x{{\d+}}, x{{\d+}}, lsl #32
  /// CHECK-NEXT:            asr  x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NEXT:            cinc w{{\d+}}, w{{\d+}}, mi
  private static int $noinline$IntDivByMinus7(int v) {
    int r = v / -7;
    return r;
  }

  // A test case to check that 'asr' is used to get the high 32 bits of the result of
  // 'dividend * magic'.
  // For divisor 6 seen in the core library there is no need to correct the result of
  // get_high(dividend * magic). Also there is no 'asr' before the final 'add' instruction
  // which uses only the high 32 bits of the result. In such a case 'asr' getting the high
  // 32 bits can be used as well.
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntDivBy6(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntDivBy6(int v) {
    int r = v / 6;
    return r;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.$noinline$IntALenDivBy6(int[]) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntALenDivBy6(int[]) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntALenDivBy6(int[] arr) {
    int r = arr.length / 6;
    return r;
  }

  // A test case to check that 'asr' is used to get the high 32 bits of the result of
  // 'dividend * magic'.
  // Divisor -6 has the same property as divisor 6: no need to correct the result of
  // get_high(dividend * magic) and no 'asr' before the final 'add' instruction
  // which uses only the high 32 bits of the result. In such a case 'asr' getting the high
  // 32 bits can be used as well.
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntDivByMinus6(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntDivByMinus6(int v) {
    int r = v / -6;
    return r;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.$noinline$IntIndVarDivBy6(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.$noinline$IntIndVarDivBy6(int) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int $noinline$IntIndVarDivBy6(int v) {
    int c = 0;
    for (int i = 0; i < v; ++i) {
      c += i / 6;
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.intIndVarDivBy6T02(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.intIndVarDivBy6T02(int) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int intIndVarDivBy6T02(int v) {
    int c = 0;
    for (int i = 0; i < v; ++i) {
      for (int j = 0; j < i; ++j) {
        c += j / 6;
      }
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.intIndVarDivBy6T03(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.intIndVarDivBy6T03(int) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int intIndVarDivBy6T03(int v) {
    int c = 0;
    for (int i = 0; i < v; ++i) {
      for (int j = i; j < v; ++j) {
        c += j / 6;
      }
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.intIndVarDivBy6T04(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.intIndVarDivBy6T04(int) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int intIndVarDivBy6T04(int v) {
    int c = 0;
    for (int i = 0; i < v; ) {
      c += i / 6;
      if ((c & 1) == 0) {
        c = Math.incrementExact(c);
        ++i;
      }
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM:   int DivTest.intIndVarDivBy6T05(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            add{{s?}} r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            lsr{{s?}} r{{\d+}}, r{{\d+}}, #2
  /// CHECK-NOT:             sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.intIndVarDivBy6T05(int) disassembly (after)
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NOT:             add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  /// CHECK:                 lsr x{{\d+}}, x{{\d+}}, #34
  /// CHECK-NOT:             cinc w{{\d+}}, w{{\d+}}, mi
  private static int intIndVarDivBy6T05(int v) {
    int c = 0;
    int i = 0;
    for (; i < v; ++i) {
      c += i / 6;
    }
    c += i / 7;
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated. If v is MIN_INT32, 'i += 2' can overflow and become negative.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter01(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter01(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter01(int v) {
    int c = 0;
    for (int i = 0; i < v; i += 2) {
      c += i / 6;
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated. 'i' is initialized with a negative value.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter02(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter02(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter02(int v) {
    int c = 0;
    for (int i = -10; i < v; ++i) {
      c += i / 6;
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated. If v is MIN_INT32, '++i' can overflow and become negative.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter03(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter03(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter03(int v) {
    int c = 0;
    for (int i = 0; i <= v; ++i) {
      c += i / 6;
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated. It is not possible to detect whether 'i' stays non-negative.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter04(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter04(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter04(int v) {
    int c = 0;
    for (int i = 0; c < v; ++i) {
      c += i / 6;
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter05(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter05(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter05(int v) {
    int c = 0;
    for (int i = -10; i < v; ++i) {
      for (int j = i; j < v; ++j) {
        c += j / 6;
      }
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter06(int, int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter06(int, int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter06(int v1, int v2) {
    int c = 0;
    for (int i = v1; i < v2; ++i) {
      for (int j = i; j < v2; ++j) {
        c += j / 6;
      }
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated. If v is MIN_INT32, 'i += 2' can overflow and become negative.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter07(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter07(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter07(int v) {
    int c = 0;
    for (int i = 0; i < v;) {
      c += i / 6;
      if ((c & 1) == 0) {
        i += 1;
      } else {
        i += 2;
      }
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated. If v is MIN_INT32, The second '++i' can overflow and become negative.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter08(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter08(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter08(int v) {
    int c = 0;
    for (int i = 0; i < v;) {
      c += i / 6;
      ++i;
      ++i;
    }
    return c;
  }

  // A test case to check that when a loop counter might be negative a correcting 'add'
  // is generated.
  //
  /// CHECK-START-ARM:   int DivTest.mightBeNegativeLoopCounter09(int) disassembly (after)
  /// CHECK:                 smull     r{{\d+}}, r{{\d+}}, r{{\d+}}, r{{\d+}}
  /// CHECK-NEXT:            sub       r{{\d+}}, r{{\d+}}, r{{\d+}}, asr #31
  //
  /// CHECK-START-ARM64: int DivTest.mightBeNegativeLoopCounter09(int) disassembly (after)
  /// CHECK:                 asr x{{\d+}}, x{{\d+}}, #32
  /// CHECK-NEXT:            add w{{\d+}}, w{{\d+}}, w{{\d+}}, lsr #31
  private static int mightBeNegativeLoopCounter09(int v) {
    int c = 0;
    int j = 0;
    for (int i = -10; i < v; ++i) {
      for (j = i; j < v; ++j) {
        c += 1;
      }
    }
    c += j / 6;
    return c;
  }

  private static void divLong() {
    expectEquals(0L, $noinline$LongDivBy18(0L));
    expectEquals(0L, $noinline$LongDivBy18(1L));
    expectEquals(0L, $noinline$LongDivBy18(-1L));
    expectEquals(1L, $noinline$LongDivBy18(18L));
    expectEquals(-1L, $noinline$LongDivBy18(-18L));
    expectEquals(3L, $noinline$LongDivBy18(65L));
    expectEquals(-3L, $noinline$LongDivBy18(-65L));

    expectEquals(0L, $noinline$LongDivByMinus18(0L));
    expectEquals(0L, $noinline$LongDivByMinus18(1L));
    expectEquals(0L, $noinline$LongDivByMinus18(-1L));
    expectEquals(-1L, $noinline$LongDivByMinus18(18L));
    expectEquals(1L, $noinline$LongDivByMinus18(-18L));
    expectEquals(-3L, $noinline$LongDivByMinus18(65L));
    expectEquals(3L, $noinline$LongDivByMinus18(-65L));

    expectEquals(0L, $noinline$LongDivBy7(0L));
    expectEquals(0L, $noinline$LongDivBy7(1L));
    expectEquals(0L, $noinline$LongDivBy7(-1L));
    expectEquals(1L, $noinline$LongDivBy7(7L));
    expectEquals(-1L, $noinline$LongDivBy7(-7L));
    expectEquals(3L, $noinline$LongDivBy7(22L));
    expectEquals(-3L, $noinline$LongDivBy7(-22L));

    expectEquals(0L, $noinline$LongDivByMinus7(0L));
    expectEquals(0L, $noinline$LongDivByMinus7(1L));
    expectEquals(0L, $noinline$LongDivByMinus7(-1L));
    expectEquals(-1L, $noinline$LongDivByMinus7(7L));
    expectEquals(1L, $noinline$LongDivByMinus7(-7L));
    expectEquals(-3L, $noinline$LongDivByMinus7(22L));
    expectEquals(3L, $noinline$LongDivByMinus7(-22L));

    expectEquals(0L, $noinline$LongDivBy6(0L));
    expectEquals(0L, $noinline$LongDivBy6(1L));
    expectEquals(0L, $noinline$LongDivBy6(-1L));
    expectEquals(1L, $noinline$LongDivBy6(6L));
    expectEquals(-1L, $noinline$LongDivBy6(-6L));
    expectEquals(3L, $noinline$LongDivBy6(19L));
    expectEquals(-3L, $noinline$LongDivBy6(-19L));

    expectEquals(0L, $noinline$LongDivByMinus6(0L));
    expectEquals(0L, $noinline$LongDivByMinus6(1L));
    expectEquals(0L, $noinline$LongDivByMinus6(-1L));
    expectEquals(-1L, $noinline$LongDivByMinus6(6L));
    expectEquals(1L, $noinline$LongDivByMinus6(-6L));
    expectEquals(-3L, $noinline$LongDivByMinus6(19L));
    expectEquals(3L, $noinline$LongDivByMinus6(-19L));

    expectEquals(0L, $noinline$LongDivBy100(0L));
    expectEquals(0L, $noinline$LongDivBy100(1L));
    expectEquals(0L, $noinline$LongDivBy100(-1L));
    expectEquals(1L, $noinline$LongDivBy100(100L));
    expectEquals(-1L, $noinline$LongDivBy100(-100L));
    expectEquals(3L, $noinline$LongDivBy100(301L));
    expectEquals(-3L, $noinline$LongDivBy100(-301L));

    expectEquals(0L, $noinline$LongDivByMinus100(0L));
    expectEquals(0L, $noinline$LongDivByMinus100(1L));
    expectEquals(0L, $noinline$LongDivByMinus100(-1L));
    expectEquals(-1L, $noinline$LongDivByMinus100(100L));
    expectEquals(1L, $noinline$LongDivByMinus100(-100L));
    expectEquals(-3L, $noinline$LongDivByMinus100(301L));
    expectEquals(3L, $noinline$LongDivByMinus100(-301L));

    for (int i = 1; i < 1234; ++i) {
      expectEquals($noinline$ExpectedLongIndVarDivByN(i, 6), $noinline$LongIndVarDivBy6(i));
      expectEquals($noinline$ExpectedLongIndVarDivByN(i, 7), $noinline$LongIndVarDivBy7(i));
      expectEquals($noinline$ExpectedLongIndVarDivByN(i, 18), $noinline$LongIndVarDivBy18(i));
      expectEquals($noinline$ExpectedLongIndVarDivByN(i, 100), $noinline$LongIndVarDivBy100(i));
    }
  }

  // Test cases for Int64 HDiv/HRem to check that optimizations implemented for Int32 are not
  // used for Int64. The same divisors 18, -18, 7, -7, 6 and -6 are used.

  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivBy18(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongDivBy18(long v) {
    long r = v / 18L;
    return r;
  }

  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivByMinus18(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongDivByMinus18(long v) {
    long r = v / -18L;
    return r;
  }

  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivBy7(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr x{{\d+}}, x{{\d+}}, #1
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongDivBy7(long v) {
    long r = v / 7L;
    return r;
  }

  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivByMinus7(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr x{{\d+}}, x{{\d+}}, #1
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongDivByMinus7(long v) {
    long r = v / -7L;
    return r;
  }

  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivBy6(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongDivBy6(long v) {
    long r = v / 6L;
    return r;
  }

  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivByMinus6(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongDivByMinus6(long v) {
    long r = v / -6L;
    return r;
  }

  // A test to check 'add' and 'add_shift' are optimized into 'adds' and 'cinc'.
  //
  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivBy100(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            adds  x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr   x{{\d+}}, x{{\d+}}, #6
  /// CHECK-NEXT:            cinc  x{{\d+}}, x{{\d+}}, mi
  private static long $noinline$LongDivBy100(long v) {
    long r = v / 100L;
    return r;
  }

  // A test to check 'subs' and 'add_shift' are optimized into 'subs' and 'cinc'.
  //
  /// CHECK-START-ARM64: long DivTest.$noinline$LongDivByMinus100(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            subs  x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            asr   x{{\d+}}, x{{\d+}}, #6
  /// CHECK-NEXT:            cinc  x{{\d+}}, x{{\d+}}, mi
  private static long $noinline$LongDivByMinus100(long v) {
    long r = v / -100L;
    return r;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM64: long DivTest.$noinline$LongIndVarDivBy6(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NOT:             add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongIndVarDivBy6(long v) {
    long c = 0;
    for (long i = 0; i < v; ++i) {
      c += i / 6;
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM64: long DivTest.$noinline$LongIndVarDivBy7(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            lsr x{{\d+}}, x{{\d+}}, #1
  /// CHECK-NOT:             add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongIndVarDivBy7(long v) {
    long c = 0;
    for (long i = 0; i < v; ++i) {
      c += i / 7;
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM64: long DivTest.$noinline$LongIndVarDivBy18(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NOT:             add x{{\d+}}, x{{\d+}}, x{{\d+}}, lsr #63
  private static long $noinline$LongIndVarDivBy18(long v) {
    long c = 0;
    for (long i = 0; i < v; ++i) {
      c += i / 18;
    }
    return c;
  }

  // A test case to check that a correcting 'add' is not generated for a non-negative
  // dividend and a positive divisor.
  //
  /// CHECK-START-ARM64: long DivTest.$noinline$LongIndVarDivBy100(long) disassembly (after)
  /// CHECK:                 smulh x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            add   x{{\d+}}, x{{\d+}}, x{{\d+}}
  /// CHECK-NEXT:            lsr   x{{\d+}}, x{{\d+}}, #6
  private static long $noinline$LongIndVarDivBy100(long v) {
    long c = 0;
    for (long i = 0; i < v; ++i) {
      c += i / 100;
    }
    return c;
  }

  private static long $noinline$ExpectedLongIndVarDivByN(long v, int n) {
    long c = 0;
    for (long i = 0; i < v; ++i) {
      c += i / n;
    }
    return c;
  }
}
