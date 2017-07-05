/*
 * Copyright (C) 2016 The Android Open Source Project
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

package art;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;

public class Test1900 {
  public static void checkEq(long exp, long o) {
    if (exp != o) {
      throw new Error("Expected: " + exp + " Got: " + o);
    }
  }

  public static void runConcurrent(Runnable... rs) throws Exception {
    final CountDownLatch latch = new CountDownLatch(rs.length);
    Thread[] thrs = new Thread[rs.length];
    for (int i = 0; i < rs.length; i++) {
      final Runnable r = rs[i];
      thrs[i] = new Thread(() -> {
        latch.countDown();
        r.run();
      });
      thrs[i].start();
    }
    for (Thread thr : thrs) {
      thr.join();
    }
  }
  static class Holder {
    public long val;
  }

  private static void checkAlloc(long exp_alloc, long exp_dealloc) {
    checkEq(exp_alloc, getAmountAllocated());
    checkEq(exp_dealloc, getAmountFreed());
  }
  public static void run() throws Exception {
    initializeTest();
    long abc = doAllocate(10);
    long def = doAllocate(20);
    try {
      getAmountAllocated();
      throw new Error("Unexpected no error for getting allocated before tracking started");
    } catch (Exception e) {
      if (!e.getMessage().equals("JVMTI_ERROR_ABSENT_INFORMATION")) {
        throw new Error("Unexpected error for getting allocated before tracking started", e);
      }
    }

    long exp_alloc = 0;
    long exp_dealloc = 0;

    doDeallocate(abc);
    startTrackingAllocations();
    checkAlloc(exp_alloc, exp_dealloc);

    // Deallocating something from before we started tracking has no effect.
    doDeallocate(def);
    checkEq(exp_dealloc, getAmountFreed());

    // Basic alloc-dealloc
    abc = doAllocate(100);
    exp_alloc += 100;
    checkAlloc(exp_alloc, exp_dealloc);

    doDeallocate(abc);
    exp_dealloc += 100;
    checkAlloc(exp_alloc, exp_dealloc);

    // Try doing it concurrently.
    Runnable add10 = () -> { long x = doAllocate(10); doDeallocate(x); };
    Runnable[] rs = new Runnable[100];
    Arrays.fill(rs, add10);
    runConcurrent(rs);
    exp_alloc += 10*100;
    exp_dealloc += 10*100;
    checkAlloc(exp_alloc, exp_dealloc);

    // Try doing it concurrently with different threads to allocate and deallocate.
    final Semaphore sem = new Semaphore(0);
    final Holder h = new Holder();
    runConcurrent(
        () -> {
          try {
            h.val = doAllocate(100);
            sem.release();
          } catch (Exception e) { throw new Error("exception!", e); }
        },
        () -> {
          try {
            sem.acquire();
            doDeallocate(h.val);
          } catch (Exception e) { throw new Error("exception!", e); }
        }
    );
    exp_alloc += 100;
    exp_dealloc += 100;
    checkAlloc(exp_alloc, exp_dealloc);
  }

  private static native long doAllocate(long size);
  private static native long doDeallocate(long ptr);
  private static native long getAmountFreed();
  private static native long getAmountAllocated();
  private static native void startTrackingAllocations();
  private static native void initializeTest();
}
