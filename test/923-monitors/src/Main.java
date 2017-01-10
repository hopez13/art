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

import java.util.concurrent.CountDownLatch;
import java.util.ArrayList;
import java.util.List;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[1]);

    doTest();
  }

  private static void doTest() throws Exception {
    // Start a watchdog, to make sure on deadlocks etc the test dies.
    startWatchdog();

    sharedId = createRawMonitor();

    simpleTests(sharedId);

    threadTests(sharedId);

    destroyRawMonitor(sharedId);
  }

  private static void simpleTests(long id) {
    unlock(id);  // Should fail.

    lock(id);
    unlock(id);
    unlock(id);  // Should fail.

    lock(id);
    lock(id);
    unlock(id);
    unlock(id);
    unlock(id);  // Should fail.

    rawWait(id, 0);   // Should fail.
    rawWait(id, -1);  // Should fail.
    rawWait(id, 1);   // Should fail.

    lock(id);
    rawWait(id, 50);
    unlock(id);
    unlock(id);  // Should fail.

    rawNotify(id);  // Should fail.
    lock(id);
    rawNotify(id);
    unlock(id);
    unlock(id);  // Should fail.

    rawNotifyAll(id);  // Should fail.
    lock(id);
    rawNotifyAll(id);
    unlock(id);
    unlock(id);  // Should fail.
  }

  private static void threadTests(final long id) throws Exception {
    final int N = 10;

    final CountDownLatch waitLatch = new CountDownLatch(N);
    final CountDownLatch wait2Latch = new CountDownLatch(1);

    Runnable r = new Runnable() {
      @Override
      public void run() {
        lock(id);
        waitLatch.countDown();
        rawWait(id, 0);
        unlock(id);
        System.out.println("Awakened");
        wait2Latch.countDown();
      }
    };

    List<Thread> threads = new ArrayList<Thread>();
    for (int i = 0; i < N; i++) {
      Thread t = new Thread(r);
      threads.add(t);
      t.start();
    }

    Thread current = Thread.currentThread();

    // Wait till all threads have been started.
    waitLatch.await();

    // Hopefully enough time for all the threads to progress into wait.
    current.yield();
    current.sleep(500);

    // Wake up one.
    lock(id);
    rawNotify(id);
    unlock(id);

    wait2Latch.await();

    // Wait a little bit more to see stragglers. This is flaky - spurious wakeups could
    // make the test fail.
    current.yield();
    current.sleep(500);

    // Wake up everyone else.
    lock(id);
    rawNotifyAll(id);
    unlock(id);

    // Wait for everyone to die.
    for (Thread t : threads) {
      t.join();
    }
  }

  private static void lock(long id) {
    System.out.println("Lock");
    rawMonitorEnter(id);
  }

  private static void unlock(long id) {
    System.out.println("Unlock");
    try {
      rawMonitorExit(id);
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
  }

  private static void rawWait(long id, long millis) {
    System.out.println("Wait");
    try {
      rawMonitorWait(id, millis);
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
  }

  private static void rawNotify(long id) {
    System.out.println("Notify");
    try {
      rawMonitorNotify(id);
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
  }

  private static void rawNotifyAll(long id) {
    System.out.println("NotifyAll");
    try {
      rawMonitorNotifyAll(id);
    } catch (RuntimeException e) {
      System.out.println(e.getMessage());
    }
  }

  private static void startWatchdog() {
    Runnable r = new Runnable() {
      @Override
      public void run() {
        long start = System.currentTimeMillis();
        // Give it a minute.
        long end = 60 * 1000 + start;
        for (;;) {
          long delta = end - System.currentTimeMillis();
          if (delta <= 0) {
            break;
          }

          try {
            Thread.currentThread().sleep(delta);          
          } catch (Exception e) {
          }
        }
        System.out.println("TIMEOUT!");
        System.exit(1);
      }
    };
    Thread t = new Thread(r);
    t.setDaemon(true);
    t.start();
  }

  static volatile long sharedId;

  private static native long createRawMonitor();
  private static native void destroyRawMonitor(long id);
  private static native void rawMonitorEnter(long id);
  private static native void rawMonitorExit(long id);
  private static native void rawMonitorWait(long id, long millis);
  private static native void rawMonitorNotify(long id);
  private static native void rawMonitorNotifyAll(long id);
}
