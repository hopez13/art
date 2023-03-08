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
        $noinline$testSetGetValue();
        $noinline$testTernaryOperator();
        $noinline$testEqualToItself();
        $noinline$testEqualToItselfPlus256();
    }

    private static void $noinline$testSetGetValue() throws Throwable {
        Class<?> c = Class.forName("BooleanTest");

        // The setGetBoolean methods store and load a byte. In that vein, a boolean value is false
        // iff the lowest significant byte is 0 (i.e it is equal to 0 modulo 256).
        // clang-format off
        assertFalse((Boolean) c.getMethod("setGetBooleanConst0").invoke(null, null),   0);
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst1").invoke(null, null),   1);
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst2").invoke(null, null),   2);
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst255").invoke(null, null), 255);
        assertFalse((Boolean) c.getMethod("setGetBooleanConst256").invoke(null, null), 256);
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst257").invoke(null, null), 257);
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst511").invoke(null, null), 511);
        assertFalse((Boolean) c.getMethod("setGetBooleanConst512").invoke(null, null), 512);
        assertTrue ((Boolean) c.getMethod("setGetBooleanConst513").invoke(null, null), 513);
        // clang-format on

        // Same thing but with a int parameter instead of constant value.
        Method m = c.getDeclaredMethod("setGetBoolean", int.class);
        for (int i = 0; i <= 513; i++) {
            if (i % 256 == 0) {
                assertFalse((Boolean) m.invoke(null, i), i);
            } else {
                assertTrue((Boolean) m.invoke(null, i), i);
            }
        }
    }

    // Tests that we return only 1 or 0, and not other values e.g. 2.
    private static void $noinline$testTernaryOperator() throws Throwable {
        Class<?> c = Class.forName("BooleanTest");
        Method m = c.getDeclaredMethod("ternaryOperatorProxy", int.class);
        // TODO(solanes): Change 255 to 513 once we fix the comparison.
        for (int i = 0; i <= 255; i++) {
            if (i % 256 == 0) {
                assertEquals(0, (Integer) m.invoke(null, i), i);
            } else {
                assertEquals(1, (Integer) m.invoke(null, i), i);
            }
        }
    }

    // Tests that booleans are equal to themselves after setting and reading a boolean value.
    private static void $noinline$testEqualToItself() throws Throwable {
        Class<?> c = Class.forName("BooleanTest");
        Method m = c.getDeclaredMethod("equalToItselfProxy", int.class);
        // TODO(solanes): Change 255 to 513 once we fix the comparison.
        for (int i = 0; i <= 255; i++) {
            assertTrue((Boolean) m.invoke(null, i), i);
        }
    }

    // Tests that booleans are equal to themselves plus 256.
    private static void $noinline$testEqualToItselfPlus256() throws Throwable {
        Class<?> c = Class.forName("BooleanTest");
        Method m = c.getDeclaredMethod("equalBooleansProxy", int.class, int.class);
        // TODO(solanes): Change -1 to 513 once we fix the comparison.
        for (int i = 0; i <= -1; i++) {
            assertTrue((Boolean) m.invoke(null, i, i + 256), i);
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

    private static void assertEquals(int expected, int actual, int iteration) {
        if (expected != actual) {
            throw new AssertionError(
                    "Expected " + expected + " got " + actual + " for " + iteration);
        }
    }

    public static boolean field;
}
