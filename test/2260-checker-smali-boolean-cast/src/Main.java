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

import java.lang.reflect.Method;

public class Main {
    public static void main(String[] args) throws Throwable {
        Class<?> c = Class.forName("BooleanTest");

        // A boolean value is false iff the lowest significant byte is 0 (i.e it is equal to 0
        // modulo 256).
        // clang-format off
        assertFalse((Boolean) c.getMethod("setGetBooleanConst0").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst1").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst2").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst255").invoke(null, null));
        assertFalse((Boolean) c.getMethod("setGetBooleanConst256").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst257").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst511").invoke(null, null));
        assertFalse((Boolean) c.getMethod("setGetBooleanConst512").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst513").invoke(null, null));
        // clang-format on

        // Same thing but with a int parameter instead of constant value
        Method m = c.getDeclaredMethod("setGetBoolean", int.class);
        for (int i = 0; i <= 513; i++) {
            if (i % 256 == 0) {
                assertFalse((Boolean) m.invoke(null, i));
            } else {
                assertTrue((Boolean) m.invoke(null, i));
            }
        }
    }

    private static void assertTrue(boolean result) {
        if (!result) {
            throw new Error("Expected true");
        }
    }

    private static void assertFalse(boolean result) {
        if (result) {
            throw new Error("Expected false");
        }
    }

    public static boolean field;
}
