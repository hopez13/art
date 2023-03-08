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
    public static boolean field;

    public static void assertTrue(boolean result) {
        if (!result) {
            throw new Error("Expected true");
        }
    }

    public static void assertFalse(boolean result) {
        if (result) {
            throw new Error("Expected false");
        }
    }

    public static void main(String[] args) throws Throwable {
        Class<?> c = Class.forName("BooleanTest");

        // clang-format off

        // A boolean value is false iff the lowest significant bit is 0 (i.e it is equal
        // to 0 modulo 256).
        assertFalse((Boolean) c.getMethod("setGetBooleanConst0").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst1").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst2").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst255").invoke(null, null));
        assertFalse((Boolean) c.getMethod("setGetBooleanConst256").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst257").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst511").invoke(null, null));
        assertFalse((Boolean) c.getMethod("setGetBooleanConst512").invoke(null, null));
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst513").invoke(null, null));

        // Same thing but with a int parameter instead of constant value
        Method m = c.getDeclaredMethod("setGetBoolean", int.class);
        assertFalse((Boolean) m.invoke(null, 0));
        assertTrue ((Boolean) m.invoke(null, 1));
        assertTrue ((Boolean) m.invoke(null, 2));
        assertTrue ((Boolean) m.invoke(null, 255));
        assertFalse((Boolean) m.invoke(null, 256));
        assertTrue ((Boolean) m.invoke(null, 257));
        assertTrue ((Boolean) m.invoke(null, 511));
        assertFalse((Boolean) m.invoke(null, 512));
        assertTrue ((Boolean) m.invoke(null, 513));

        // clang-format on
    }
}
