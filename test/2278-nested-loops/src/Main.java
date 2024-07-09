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

    // Same as doing:
    //   result = initial_value;
    //   for (int i = start; i < end; ++i) { result += i * 10; }
    //   return result;
    // As long as `initial_value` is 5 or less
    private static int $noinline$nestedLoopCalculation(int start, int end, int initial_value) {
        int result = initial_value;
        for (int outer = start; outer < end; ++outer) {
            int first_inner;
            for (first_inner = 5; first_inner > outer; first_inner--)
                ;
            for (int second_inner = 0; second_inner < 10; second_inner++)
                result += first_inner;
        }
        return result;
    }

    public static void main(String[] f) {
        // 12 + 0 + 10 = 22
        assertIntEquals(22, $noinline$nestedLoopCalculation(0, 2, 12));
        // 12 + 10 + 20 = 42
        assertIntEquals(42, $noinline$nestedLoopCalculation(1, 3, 12));
        // 12 + (-20) + (-10) + 0 + 10 = -8
        assertIntEquals(-8, $noinline$nestedLoopCalculation(-2, 2, 12));
        // 12 + 0 + 10 + 20 + 30 + 40 = 112
        assertIntEquals(112, $noinline$nestedLoopCalculation(0, 5, 12));
    }

    public static void assertIntEquals(int expected, int result) {
        if (expected != result) {
            throw new Error("Expected: " + expected + ", found: " + result);
        }
    }
}