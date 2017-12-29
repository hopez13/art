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

    // Test that calling registerNativeAllocation triggers a GC eventually
    // after a substantial number of registered native bytes.
    private static void checkRegisterNativeAllocation() throws Exception {
        long maxMem = Runtime.getRuntime().maxMemory();
        int size = (int)(maxMem / 32);
        int allocation_count = 256;

        ReferenceQueue<Object> queue = new ReferenceQueue<Object>();
        PhantomReference ref = new PhantomReference(new Object(), queue);
        long total = 0;
        for (int i = 0; !ref.isEnqueued() && i < allocation_count; ++i) {
            runtime.registerNativeAllocation(size);
            total += size;
        }

        // Wait up to 1000ms to give GC a chance to finish running.
        if (queue.remove(1000) == null) {
            throw new RuntimeException("GC failed to run");
        }

        while (total > 0) {
            runtime.registerNativeFree(size);
            total -= size;
        }
    }

    public static void main(String[] args) throws Exception {
        // Test that registerNativeAllocation triggers GC, and continues to
        // trigger GC if we continue to do more allocations.
        for (int i = 0; i < 4; ++i) {
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

