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
    public static void main(String[] args) {}
    public static void $inline$empty() {}
    public static void $inline$empty2() {}

    /// CHECK-START: void Main.andCall2(boolean, boolean) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.andCall2(boolean, boolean) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void andCall2(boolean a, boolean b) {
        if (a && b) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    /// CHECK-START: void Main.andCall3(boolean, boolean, boolean) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.andCall3(boolean, boolean, boolean) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void andCall3(boolean a, boolean b, boolean c) {
        if (a && b && c) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    /// CHECK-START: void Main.andCall4(boolean, boolean, boolean, boolean) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.andCall4(boolean, boolean, boolean, boolean) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void andCall4(boolean a, boolean b, boolean c, boolean d) {
        if (a && b && c && d) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    /// CHECK-START: void Main.orCall2(boolean, boolean) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.orCall2(boolean, boolean) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void orCall2(boolean a, boolean b) {
        if (a || b) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    /// CHECK-START: void Main.orCall3(boolean, boolean, boolean) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.orCall3(boolean, boolean, boolean) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void orCall3(boolean a, boolean b, boolean c) {
        if (a || b || c) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    /// CHECK-START: void Main.orCall4(boolean, boolean, boolean, boolean) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.orCall4(boolean, boolean, boolean, boolean) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void orCall4(boolean a, boolean b, boolean c, boolean d) {
        if (a || b || c || d) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    /// CHECK-START: void Main.intCall(int, int, int, int) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.intCall(int, int, int, int) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    public static void intCall(int a, int b, int c, int d) {
        if (a <= b && c <= d && a >= 20 && b <= 78 && c >= 50 && d <= 70) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }

    class MyObject {
        boolean inner;
        Object inner2;
    }

    /// CHECK-START: void Main.objectCall(Main$MyObject) dead_code_elimination$after_inlining (before)
    /// CHECK: If
    /// CHECK: If

    /// CHECK-START: void Main.objectCall(Main$MyObject) dead_code_elimination$after_inlining (before)
    /// CHECK: InstanceFieldGet

    /// CHECK-START: void Main.objectCall(Main$MyObject) dead_code_elimination$after_inlining (after)
    /// CHECK-NOT: If
    /// CHECK-NOT: InstanceFieldGet
    public static void objectCall(MyObject o) {
        if (o != null && o.inner) {
            $inline$empty();
        } else {
            $inline$empty2();
        }
    }
}
