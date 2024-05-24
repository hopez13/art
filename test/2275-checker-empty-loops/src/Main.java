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
    static int static_int = 0;
    static long static_long = 0;

    public static void main(String[] args) {
        $noinline$testSameStartEndInt();
        $noinline$testStartBiggerThanEndInt();
        $noinline$testUnknownParameterValuesInt(2, 1);
        $noinline$testKnownParameterValuesInt();

        $noinline$testSameStartEndLong();
        $noinline$testStartBiggerThanEndLong();
        $noinline$testUnknownParameterValuesLong(2, 1);
        $noinline$testKnownParameterValuesLong();
    }

    /// CHECK-START: void Main.$noinline$testSameStartEndInt() loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testSameStartEndInt() loop_optimization (after)
    /// CHECK: <<Const1:i\d+>> IntConstant 1
    /// CHECK:                 If [<<Const1>>]

    /// CHECK-START: void Main.$noinline$testSameStartEndInt() dead_code_elimination$after_loop_opt (before)
    /// CHECK:     Phi
    /// CHECK:     GreaterThanOrEqual
    /// CHECK:     If
    /// CHECK:     StaticFieldSet

    /// CHECK-START: void Main.$noinline$testSameStartEndInt() dead_code_elimination$after_loop_opt (after)
    /// CHECK-NOT: Phi
    /// CHECK-NOT: GreaterThanOrEqual
    /// CHECK-NOT: If
    /// CHECK-NOT: StaticFieldSet
    private static void $noinline$testSameStartEndInt() {
        for (int q = 2; q < 2; q++) {
            static_int = 24;
        }
    }

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndInt() loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndInt() loop_optimization (after)
    /// CHECK: <<Const1:i\d+>> IntConstant 1
    /// CHECK:                 If [<<Const1>>]

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndInt() dead_code_elimination$after_loop_opt (before)
    /// CHECK:     Phi
    /// CHECK:     GreaterThanOrEqual
    /// CHECK:     If
    /// CHECK:     StaticFieldSet

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndInt() dead_code_elimination$after_loop_opt (after)
    /// CHECK-NOT: Phi
    /// CHECK-NOT: GreaterThanOrEqual
    /// CHECK-NOT: If
    /// CHECK-NOT: StaticFieldSet
    public static void $noinline$testStartBiggerThanEndInt() {
        for (int q = 2; q < 1; q++) {
            static_int = 24;
        }
    }

    // Since the parameters are unknown, we have to keep the loop.

    /// CHECK-START: void Main.$noinline$testUnknownParameterValuesInt(int, int) loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testUnknownParameterValuesInt(int, int) loop_optimization (after)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]
    public static void $noinline$testUnknownParameterValuesInt(int start, int end) {
        for (int q = start; q < end; q++) {
            static_int = 24;
        }
    }

    public static void $inline$unknownParameterValuesInt(int start, int end) {
        for (int q = start; q < end; q++) {
            static_int = 24;
        }
    }

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesInt() loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesInt() loop_optimization (after)
    /// CHECK: <<Const1:i\d+>> IntConstant 1
    /// CHECK:                 If [<<Const1>>]

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesInt() dead_code_elimination$after_loop_opt (before)
    /// CHECK:     Phi
    /// CHECK:     GreaterThanOrEqual
    /// CHECK:     If
    /// CHECK:     StaticFieldSet

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesInt() dead_code_elimination$after_loop_opt (after)
    /// CHECK-NOT: Phi
    /// CHECK-NOT: GreaterThanOrEqual
    /// CHECK-NOT: If
    /// CHECK-NOT: StaticFieldSet
    public static void $noinline$testKnownParameterValuesInt() {
        $inline$unknownParameterValuesInt(2, 1);
    }

    /// CHECK-START: void Main.$noinline$testSameStartEndLong() loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testSameStartEndLong() loop_optimization (after)
    /// CHECK: <<Const1:i\d+>> IntConstant 1
    /// CHECK:                 If [<<Const1>>]

    /// CHECK-START: void Main.$noinline$testSameStartEndLong() dead_code_elimination$after_loop_opt (before)
    /// CHECK:     Phi
    /// CHECK:     GreaterThanOrEqual
    /// CHECK:     If
    /// CHECK:     StaticFieldSet

    /// CHECK-START: void Main.$noinline$testSameStartEndLong() dead_code_elimination$after_loop_opt (after)
    /// CHECK-NOT: Phi
    /// CHECK-NOT: GreaterThanOrEqual
    /// CHECK-NOT: If
    /// CHECK-NOT: StaticFieldSet
    private static void $noinline$testSameStartEndLong() {
        for (long q = 2; q < 2; q++) {
            static_long = 24;
        }
    }

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndLong() loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndLong() loop_optimization (after)
    /// CHECK: <<Const1:i\d+>> IntConstant 1
    /// CHECK:                 If [<<Const1>>]

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndLong() dead_code_elimination$after_loop_opt (before)
    /// CHECK:     Phi
    /// CHECK:     GreaterThanOrEqual
    /// CHECK:     If
    /// CHECK:     StaticFieldSet

    /// CHECK-START: void Main.$noinline$testStartBiggerThanEndLong() dead_code_elimination$after_loop_opt (after)
    /// CHECK-NOT: Phi
    /// CHECK-NOT: GreaterThanOrEqual
    /// CHECK-NOT: If
    /// CHECK-NOT: StaticFieldSet
    public static void $noinline$testStartBiggerThanEndLong() {
        for (long q = 2; q < 1; q++) {
            static_long = 24;
        }
    }

    // Since the parameters are unknown, we have to keep the loop.

    /// CHECK-START: void Main.$noinline$testUnknownParameterValuesLong(long, long) loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testUnknownParameterValuesLong(long, long) loop_optimization (after)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]
    public static void $noinline$testUnknownParameterValuesLong(long start, long end) {
        for (long q = start; q < end; q++) {
            static_long = 24;
        }
    }

    public static void $inline$unknownParameterValuesLong(long start, long end) {
        for (long q = start; q < end; q++) {
            static_long = 24;
        }
    }

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesLong() loop_optimization (before)
    /// CHECK: <<Comp:z\d+>> GreaterThanOrEqual
    /// CHECK:               If [<<Comp>>]

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesLong() loop_optimization (after)
    /// CHECK: <<Const1:i\d+>> IntConstant 1
    /// CHECK:                 If [<<Const1>>]

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesLong() dead_code_elimination$after_loop_opt (before)
    /// CHECK:     Phi
    /// CHECK:     GreaterThanOrEqual
    /// CHECK:     If
    /// CHECK:     StaticFieldSet

    /// CHECK-START: void Main.$noinline$testKnownParameterValuesLong() dead_code_elimination$after_loop_opt (after)
    /// CHECK-NOT: Phi
    /// CHECK-NOT: GreaterThanOrEqual
    /// CHECK-NOT: If
    /// CHECK-NOT: StaticFieldSet
    public static void $noinline$testKnownParameterValuesLong() {
        $inline$unknownParameterValuesLong(2, 1);
    }
}