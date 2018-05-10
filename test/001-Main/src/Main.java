/*
 * Copyright (C) 2011 The Android Open Source Project
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
    public static void main(String args[]) throws Exception {
        System.loadLibrary(args[0]);
        final Main m0 = new Main();
        final Main m = new Main();
        Thread t = new Thread() {
            public void run() {
                m0.stop = true;
                Main m1 = m;  // `m` is in a field of this object, bring it to a local variable.
                // Triggering checkpoints in this loop.
                while (!m1.stop) {
                    ++m1.counter;
                }
            }
        };
        t.start();
        // Wait for `t` to enter Java code.
        while (!m0.stop) { }
        m.benchmarkSuspend(t);
        m.stop = true;
        t.join();
    }

    public volatile boolean stop = false;
    public volatile int counter = 0;
    public native void benchmarkSuspend(Thread t);
}
