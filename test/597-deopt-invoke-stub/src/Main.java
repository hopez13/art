/*
 * Copyright (C) 2017 The Android Open Source Project
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

public class Main implements Runnable {
    static final int numberOfThreads = 2;
    volatile static boolean sExitFlag1 = false;
    volatile static boolean sExitFlag2 = false;
    volatile static boolean sEntered1 = false;
    volatile static boolean sEntered2 = false;
    int threadIndex;

    private static native void deoptimizeAll();
    private static native void assertIsInterpreted();
    private static native void assertIsManaged();
    private static native void ensureJitCompiled(Class<?> cls, String methodName);

    Main(int index) {
        threadIndex = index;
    }

    public static void main(String[] args) throws Exception {
        System.loadLibrary(args[0]);

        final Thread[] threads = new Thread[numberOfThreads];
        for (int t = 0; t < threads.length; t++) {
            threads[t] = new Thread(new Main(t));
            threads[t].start();
        }
        for (Thread t : threads) {
            t.join();
        }
        System.out.println("Finishing");
    }

    private static int $noinline$bar() {
        // Should be entered via interpreter bridge.
        assertIsInterpreted();
        sEntered2 = true;
        while (!sExitFlag2) {}
        assertIsInterpreted();
        return 0x1234;
    }

    public void $noinline$foo() {
        assertIsManaged();
        sEntered1 = true;
        while (!sExitFlag1) {
            if ($noinline$bar() != 0x1234) {
                System.out.println("Bad return value");
            }
        }
        assertIsInterpreted();
    }

    public void run() {
        if (threadIndex == 0) {
            while (!sEntered1) {
              Thread.yield();
            }
            sExitFlag1 = true;
            while (!sEntered2) {
              Thread.yield();
            }
            deoptimizeAll();
            sExitFlag2 = true;
        } else {
            ensureJitCompiled(Main.class, "$noinline$foo");
            $noinline$foo();
        }
    }
}
