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
        // clang-format off
        assertFalse(BooleanTest.$noinline$setGetBooleanConst0(),   0);
        assertTrue (BooleanTest.$noinline$setGetBooleanConst1(),   1);
        assertTrue (BooleanTest.$noinline$setGetBooleanConst2(),   2);
        assertTrue (BooleanTest.$noinline$setGetBooleanConst255(), 255);
        assertFalse(BooleanTest.$noinline$setGetBooleanConst256(), 256);
        assertTrue (BooleanTest.$noinline$setGetBooleanConst257(), 257);
        assertTrue (BooleanTest.$noinline$setGetBooleanConst511(), 511);
        assertFalse(BooleanTest.$noinline$setGetBooleanConst512(), 512);
        assertTrue (BooleanTest.$noinline$setGetBooleanConst513(), 513);
        // clang-format on

        // Same thing but with a int parameter instead of constant value.
        for (int i = 0; i <= 513; i++) {
            if (i % 256 == 0) {
                assertFalse(BooleanTest.$noinline$setGetBoolean(i), i);
            } else {
                assertTrue(BooleanTest.$noinline$setGetBoolean(i), i);
            }
        }
    }

    private static void assertTrue(boolean result, int number) {
        if (!result) {
            throw new Error("Expected true for " + number);
        }
    }

    private static void assertFalse(boolean result, int number) {
        if (result) {
            throw new Error("Expected false for " + number);
        }
    }

    public static boolean field;
}
