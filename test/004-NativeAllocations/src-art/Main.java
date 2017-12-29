/*
 * Copyright (C) 2013 The Android Open Source Project
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

import java.lang.Runtime;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.PhantomReference;
import dalvik.system.VMRuntime;

public class Main {
    static Object deadlockLock = new Object();
    static VMRuntime runtime = VMRuntime.getRuntime();
    static volatile boolean aboutToDeadlock = false;

    static class DeadlockingFinalizer {
        protected void finalize() throws Exception {
            aboutToDeadlock = true;
            synchronized (deadlockLock) { }
        }
    }

    private static void allocateDeadlockingFinalizer() {
        new DeadlockingFinalizer();
    }

    public static PhantomReference allocPhantom(ReferenceQueue<Object> queue) {
        return new PhantomReference(new Object(), queue);
    }

    // Test that calling registerNativeAllocation triggers a GC eventually
    // after a substantial number of registered native bytes.
    private static void checkRegisterNativeAllocation() throws Exception {
        long maxMem = Runtime.getRuntime().maxMemory();
        int size = (int)(maxMem / 32);
        int allocation_count = 256;

        // Do an explicit blocking GC to ensure GC is not running when we
        // start calling registerNativeAllocation. Otherwise all the calls to
        // registerNativeAllocation may complete before GC is finished and has
        // a chance to be triggered again by registerNativeAllocation.
        Runtime.getRuntime().gc();

        ReferenceQueue<Object> queue = new ReferenceQueue<Object>();
        PhantomReference ref = allocPhantom(queue);
        long total = 0;
        for (int i = 0; !ref.isEnqueued() && i < allocation_count; ++i) {
            runtime.registerNativeAllocation(size);
            total += size;
        }

        // Wait up to 2s to give GC a chance to finish running.
        // If the reference isn't enqueued after that, then it is pretty
        // unlikely (though technically still possible) that GC was triggered
        // as intended.
        if (queue.remove(2000) == null) {
            throw new RuntimeException("GC failed to complete");
        }

        while (total > 0) {
            runtime.registerNativeFree(size);
            total -= size;
        }
    }

    public static void main(String[] args) throws Exception {
        // Test that registerNativeAllocation triggers GC.
        // Run this a few times in a loop to reduce the chances that the test
        // is flaky and make sure registerNativeAllocation continues to work
        // after the first GC is triggered.
        for (int i = 0; i < 20; ++i) {
            checkRegisterNativeAllocation();
        }

        // Test that we don't get a deadlock if we call
        // registerNativeAllocation with a blocked finalizer.
        synchronized (deadlockLock) {
            allocateDeadlockingFinalizer();
            while (!aboutToDeadlock) {
                checkRegisterNativeAllocation();
            }

            // Do more allocations now that the finalizer thread is deadlocked so that we force
            // finalization and timeout.
            checkRegisterNativeAllocation();
        }
        System.out.println("Test complete");
    }
}

