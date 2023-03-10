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
        System.loadLibrary(args[0]);

        Class<?> c = Class.forName("TernaryOperatorTest");
        Method m = c.getDeclaredMethod("ternaryOperatorProxy", int.class);

        // 0 should return 0.
        assertEquals(0, (Integer) m.invoke(null, 0), 0);

        // All other values should return 1.
        for (int i = 1; i <= 513; i++) {
            assertEquals(1, (Integer) m.invoke(null, i), i);
        }
    }

    private static void assertEquals(int expected, int actual, int iteration) {
        if (expected != actual) {
            throw new AssertionError(
                    "Expected " + expected + " got " + actual + " for " + iteration);
        }
    }
}
