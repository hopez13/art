/*
 * Copyright (C) 2023 The Android Open Source Project
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
    public static void main(String[] args) {
        // Note that cases like (x + y) - y = x, and (x + y) - x = y were already handled and are
        // tested in 458-checker-instruction-simplification.

        int[] int_inputs = {0, 1, -1, Integer.MIN_VALUE, Integer.MAX_VALUE, 42, -9000};
        for (int x : int_inputs) {
            for (int y : int_inputs) {
                // y - (x + y) = -x
                assertEquals(-x, $noinline$testSubAdd(x, y));
                // x - (x + y) = -y.
                assertEquals(-y, $noinline$testSubAddOtherVersion(x, y));
                // (x - y) - x = -y.
                assertEquals(-y, $noinline$testSubSub(x, y));
                // x - (x - y) = y.
                assertEquals(y, $noinline$testSubSubOtherVersion(x, y));
            }
        }

        long[] long_inputs = {0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE, 0x100000000L,
                0x100000001L, -9000L, 0x0123456789ABCDEFL};
        for (long x : long_inputs) {
            for (long y : long_inputs) {
                // y - (x + y) = -x
                assertEquals(-x, $noinline$testSubAdd(x, y));
                // x - (x + y) = -y.
                assertEquals(-y, $noinline$testSubAddOtherVersion(x, y));
                // (x - y) - x = -y.
                assertEquals(-y, $noinline$testSubSub(x, y));
                // x - (x - y) = y.
                assertEquals(y, $noinline$testSubSubOtherVersion(x, y));
            }
        }

        //         long[] long_inputs = {
        // 0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE, 0x100000000L,
        // 0x100000001L, -9000L, 0x0123456789ABCDEFL};
    }

    // Int versions

    /// CHECK-START: int Main.$noinline$testSubAdd(int, int) instruction_simplifier (before)
    /// CHECK: <<x:i\d+>>   ParameterValue
    /// CHECK: <<y:i\d+>>   ParameterValue
    /// CHECK: <<add:i\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<sub:i\d+>> Sub [<<y>>,<<add>>]
    /// CHECK:              Return [<<sub>>]

    /// CHECK-START: int Main.$noinline$testSubAdd(int, int) instruction_simplifier (after)
    /// CHECK: <<x:i\d+>>   ParameterValue
    /// CHECK: <<y:i\d+>>   ParameterValue
    /// CHECK: <<add:i\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<neg:i\d+>> Neg [<<x>>]
    /// CHECK:              Return [<<neg>>]

    /// CHECK-START: int Main.$noinline$testSubAdd(int, int) instruction_simplifier (after)
    /// CHECK-NOT: Sub

    /// CHECK-START: int Main.$noinline$testSubAdd(int, int) dead_code_elimination$initial (after)
    /// CHECK-NOT: Add
    static int $noinline$testSubAdd(int x, int y) {
        return y - (x + y);
    }

    /// CHECK-START: int Main.$noinline$testSubAddOtherVersion(int, int) instruction_simplifier (before)
    /// CHECK: <<x:i\d+>>   ParameterValue
    /// CHECK: <<y:i\d+>>   ParameterValue
    /// CHECK: <<add:i\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<sub:i\d+>> Sub [<<x>>,<<add>>]
    /// CHECK:              Return [<<sub>>]

    /// CHECK-START: int Main.$noinline$testSubAddOtherVersion(int, int) instruction_simplifier (after)
    /// CHECK: <<x:i\d+>>   ParameterValue
    /// CHECK: <<y:i\d+>>   ParameterValue
    /// CHECK: <<add:i\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<neg:i\d+>> Neg [<<y>>]
    /// CHECK:              Return [<<neg>>]

    /// CHECK-START: int Main.$noinline$testSubAddOtherVersion(int, int) instruction_simplifier (after)
    /// CHECK-NOT: Sub

    /// CHECK-START: int Main.$noinline$testSubAddOtherVersion(int, int) dead_code_elimination$initial (after)
    /// CHECK-NOT: Add
    static int $noinline$testSubAddOtherVersion(int x, int y) {
        return x - (x + y);
    }

    /// CHECK-START: int Main.$noinline$testSubSub(int, int) instruction_simplifier (before)
    /// CHECK: <<x:i\d+>>    ParameterValue
    /// CHECK: <<y:i\d+>>    ParameterValue
    /// CHECK: <<sub1:i\d+>> Sub [<<x>>,<<y>>]
    /// CHECK: <<sub2:i\d+>> Sub [<<sub1>>,<<x>>]
    /// CHECK:               Return [<<sub2>>]

    /// CHECK-START: int Main.$noinline$testSubSub(int, int) instruction_simplifier (after)
    /// CHECK: <<x:i\d+>>    ParameterValue
    /// CHECK: <<y:i\d+>>    ParameterValue
    /// CHECK: <<sub1:i\d+>> Sub [<<x>>,<<y>>]
    /// CHECK: <<neg:i\d+>>  Neg [<<y>>]
    /// CHECK:               Return [<<neg>>]

    /// CHECK-START: int Main.$noinline$testSubSub(int, int) instruction_simplifier (after)
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START: int Main.$noinline$testSubSub(int, int) dead_code_elimination$initial (after)
    /// CHECK-NOT: Sub
    static int $noinline$testSubSub(int x, int y) {
        return (x - y) - x;
    }

    /// CHECK-START: int Main.$noinline$testSubSubOtherVersion(int, int) instruction_simplifier (before)
    /// CHECK: <<x:i\d+>>    ParameterValue
    /// CHECK: <<y:i\d+>>    ParameterValue
    /// CHECK: <<sub1:i\d+>> Sub [<<x>>,<<y>>]
    /// CHECK: <<sub2:i\d+>> Sub [<<x>>,<<sub1>>]
    /// CHECK:               Return [<<sub2>>]

    /// CHECK-START: int Main.$noinline$testSubSubOtherVersion(int, int) instruction_simplifier (after)
    /// CHECK: <<x:i\d+>>    ParameterValue
    /// CHECK: <<y:i\d+>>    ParameterValue
    /// CHECK: <<sub1:i\d+>> Sub [<<x>>,<<y>>]
    /// CHECK:               Return [<<y>>]

    /// CHECK-START: int Main.$noinline$testSubSubOtherVersion(int, int) instruction_simplifier (after)
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START: int Main.$noinline$testSubSubOtherVersion(int, int) dead_code_elimination$initial (after)
    /// CHECK-NOT: Sub
    static int $noinline$testSubSubOtherVersion(int x, int y) {
        return x - (x - y);
    }

    // Long versions

    /// CHECK-START: long Main.$noinline$testSubAdd(long, long) instruction_simplifier (before)
    /// CHECK: <<x:j\d+>>   ParameterValue
    /// CHECK: <<y:j\d+>>   ParameterValue
    /// CHECK: <<add:j\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<sub:j\d+>> Sub [<<y>>,<<add>>]
    /// CHECK:              Return [<<sub>>]

    /// CHECK-START: long Main.$noinline$testSubAdd(long, long) instruction_simplifier (after)
    /// CHECK: <<x:j\d+>>   ParameterValue
    /// CHECK: <<y:j\d+>>   ParameterValue
    /// CHECK: <<add:j\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<neg:j\d+>> Neg [<<x>>]
    /// CHECK:              Return [<<neg>>]

    /// CHECK-START: long Main.$noinline$testSubAdd(long, long) instruction_simplifier (after)
    /// CHECK-NOT: Sub

    /// CHECK-START: long Main.$noinline$testSubAdd(long, long) dead_code_elimination$initial (after)
    /// CHECK-NOT: Add
    static long $noinline$testSubAdd(long x, long y) {
        return y - (x + y);
    }

    /// CHECK-START: long Main.$noinline$testSubAddOtherVersion(long, long) instruction_simplifier (before)
    /// CHECK: <<x:j\d+>>   ParameterValue
    /// CHECK: <<y:j\d+>>   ParameterValue
    /// CHECK: <<add:j\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<sub:j\d+>> Sub [<<x>>,<<add>>]
    /// CHECK:              Return [<<sub>>]

    /// CHECK-START: long Main.$noinline$testSubAddOtherVersion(long, long) instruction_simplifier (after)
    /// CHECK: <<x:j\d+>>   ParameterValue
    /// CHECK: <<y:j\d+>>   ParameterValue
    /// CHECK: <<add:j\d+>> Add [<<x>>,<<y>>]
    /// CHECK: <<neg:j\d+>> Neg [<<y>>]
    /// CHECK:              Return [<<neg>>]

    /// CHECK-START: long Main.$noinline$testSubAddOtherVersion(long, long) instruction_simplifier (after)
    /// CHECK-NOT: Sub

    /// CHECK-START: long Main.$noinline$testSubAddOtherVersion(long, long) dead_code_elimination$initial (after)
    /// CHECK-NOT: Add
    static long $noinline$testSubAddOtherVersion(long x, long y) {
        return x - (x + y);
    }

    /// CHECK-START: long Main.$noinline$testSubSub(long, long) instruction_simplifier (before)
    /// CHECK: <<x:j\d+>>    ParameterValue
    /// CHECK: <<y:j\d+>>    ParameterValue
    /// CHECK: <<sub1:j\d+>> Sub [<<x>>,<<y>>]
    /// CHECK: <<sub2:j\d+>> Sub [<<sub1>>,<<x>>]
    /// CHECK:               Return [<<sub2>>]

    /// CHECK-START: long Main.$noinline$testSubSub(long, long) instruction_simplifier (after)
    /// CHECK: <<x:j\d+>>    ParameterValue
    /// CHECK: <<y:j\d+>>    ParameterValue
    /// CHECK: <<sub1:j\d+>> Sub [<<x>>,<<y>>]
    /// CHECK: <<neg:j\d+>>  Neg [<<y>>]
    /// CHECK:               Return [<<neg>>]

    /// CHECK-START: long Main.$noinline$testSubSub(long, long) instruction_simplifier (after)
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START: long Main.$noinline$testSubSub(long, long) dead_code_elimination$initial (after)
    /// CHECK-NOT: Sub
    static long $noinline$testSubSub(long x, long y) {
        return (x - y) - x;
    }

    /// CHECK-START: long Main.$noinline$testSubSubOtherVersion(long, long) instruction_simplifier (before)
    /// CHECK: <<x:j\d+>>    ParameterValue
    /// CHECK: <<y:j\d+>>    ParameterValue
    /// CHECK: <<sub1:j\d+>> Sub [<<x>>,<<y>>]
    /// CHECK: <<sub2:j\d+>> Sub [<<x>>,<<sub1>>]
    /// CHECK:               Return [<<sub2>>]

    /// CHECK-START: long Main.$noinline$testSubSubOtherVersion(long, long) instruction_simplifier (after)
    /// CHECK: <<x:j\d+>>    ParameterValue
    /// CHECK: <<y:j\d+>>    ParameterValue
    /// CHECK: <<sub1:j\d+>> Sub [<<x>>,<<y>>]
    /// CHECK:               Return [<<y>>]

    /// CHECK-START: long Main.$noinline$testSubSubOtherVersion(long, long) instruction_simplifier (after)
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START: long Main.$noinline$testSubSubOtherVersion(long, long) dead_code_elimination$initial (after)
    /// CHECK-NOT: Sub
    static long $noinline$testSubSubOtherVersion(long x, long y) {
        return x - (x - y);
    }

    private static void assertEquals(long expected, long result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }
}
