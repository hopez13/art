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
import java.util.Arrays;

public class Main {
    public static void main(String[] args) {
        int[] src = {1, 2, 3, -100, Integer.MAX_VALUE, Integer.MIN_VALUE};
        int[] dst = new int[src.length];
        $noinline$copy(src, dst);
        if (!Arrays.equals(src, dst)) {
            throw new Error();
        }
    }

    // Consistency check: there are two Sub instructions before instruction_simplifier_arm

    /// CHECK-START-ARM: void Main.$noinline$copy(int[], int[]) instruction_simplifier_ (before)
    /// CHECK:     Sub
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START-ARM: void Main.$noinline$copy(int[], int[]) instruction_simplifier_ (before)
    /// CHECK: <<HAnd:i\d+>> And [<<Left:i\d+>>,<<Right:i\d+>>]
    /// CHECK:               Sub [<<Left>>,<<HAnd>>]

    /// CHECK-START-ARM: void Main.$noinline$copy(int[], int[]) instruction_simplifier_ (after)
    /// CHECK-NOT: And

    /// CHECK-START-ARM: void Main.$noinline$copy(int[], int[]) instruction_simplifier_ (after)
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START-ARM: void Main.$noinline$copy(int[], int[]) instruction_simplifier_ (after)
    /// CHECK:     BitwiseNegatedRight kind:And

    // Consistency check: there are two Sub instructions before instruction_simplifier_arm64

    /// CHECK-START-ARM64: void Main.$noinline$copy(int[], int[]) instruction_simplifier_arm64 (before)
    /// CHECK:     Sub
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START-ARM64: void Main.$noinline$copy(int[], int[]) instruction_simplifier_arm64 (before)
    /// CHECK: <<HAnd:i\d+>> And [<<Left:i\d+>>,<<Right:i\d+>>]
    /// CHECK:               Sub [<<Left>>,<<HAnd>>]

    /// CHECK-START-ARM64: void Main.$noinline$copy(int[], int[]) instruction_simplifier_arm64 (after)
    /// CHECK-NOT: And

    /// CHECK-START-ARM64: void Main.$noinline$copy(int[], int[]) instruction_simplifier_arm64 (after)
    /// CHECK:     Sub
    /// CHECK-NOT: Sub

    /// CHECK-START-ARM64: void Main.$noinline$copy(int[], int[]) instruction_simplifier_arm64 (after)
    /// CHECK:     BitwiseNegatedRight kind:And
    static void $noinline$copy(int[] src, int[] dst) {
        for (int i = 0; i < src.length; i++) {
            dst[i] = src[i];
        }
    }
}
