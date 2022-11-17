/*
 * Copyright (C) 2022 The Android Open Source Project
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

/**
 * Smali exercise, copied from 800-smali and modified for this test case.
 */
public class Main {
    private void runTest() throws Exception {
        System.out.println("b/252804549 (instance-of)");
        Object retValue = Class.forName("B252804549")
                                  .getDeclaredMethod("compareInstanceOfWithTwo", Object.class)
                                  .invoke(null, new Object[] {new Object()});
        if (retValue == null || !retValue.equals(3)) {
            throw new Exception("Expected 3, but got " + retValue);
        }
    }

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);
        Main main = new Main();
        main.runTest();
        System.out.println("Done!");
    }

    private native static boolean isAotVerified(Class<?> cls);
    private native static boolean compiledWithOptimizing();
}
