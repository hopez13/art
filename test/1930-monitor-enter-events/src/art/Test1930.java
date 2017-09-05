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

package art;

import java.util.Arrays;
import java.util.Objects;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.*;
import java.util.ListIterator;
import java.util.function.Consumer;
import java.util.function.Function;

public class Test1930 {
  public static class LockController {
    private static enum Action { HOLD, RELEASE, NOTIFY, NOTIFY_ALL, WAIT, TIMED_SHORT, TIMED_LONG }

    public final Object lock;
    private final AtomicStampedReference<Action> action;
    private volatile Thread runner = null;
    private volatile boolean started = false;
    private volatile boolean held = false;
    private volatile int cnt = 0;

    public LockController(Object lock) {
      this.lock = lock;
      this.action = new AtomicStampedReference(Action.HOLD, 0);
    }

    public boolean IsLocked() {
      return held;
    }

    private void setAction(Action a) {
      int stamp = action.getStamp();
      // Wait for it to be HOLD before updating.
      while (!action.compareAndSet(Action.HOLD, a, stamp, stamp + 1)) {
        stamp = action.getStamp();
      }
    }

    public synchronized void DoLock() {
      if (IsLocked()) {
        throw new Error("lock is already acquired or being acquired.");
      }
      if (runner != null) {
        throw new Error("Already have thread!");
      }
      runner = new Thread(() -> {
        started = true;
        synchronized (lock) {
          held = true;
          int[] stamp_h = new int[] { -1 };
          while (true) {
            Action cur_action = action.get(stamp_h);
            int stamp = stamp_h[0];
            if (cur_action == Action.RELEASE) {
              // The other thread will deal with reseting action.
              break;
            }
            switch (cur_action) {
              case HOLD:
                Thread.yield();
                break;
              case NOTIFY:
                lock.notify();
                break;
              case NOTIFY_ALL:
                lock.notifyAll();
                break;
              case TIMED_SHORT:
                lock.wait(500);
                break;
              case TIMED_LONG:
                lock.wait(600000);
                break;
              case WAIT:
                lock.wait();
                break;
              default:
                throw new Error("Unknown action " + action);
            }
            // reset action back to hold if it isn't something else.
            action.compareAndSet(cur_action, Action.HOLD, stamp, stamp+1);
          }
        }
        held = false;
        started = false;
      }, "Locker thread " + cnt + " for " + lock);
      runner.start();
      cnt++;
    }

    public synchronized void waitForLockToBeHeld() throws Exception {
      while (!isLocked()) {}
    }

    public synchronized void waitForContendedSleep() throws Exception {
      if (runner == null) {
        throw new Error("No thread trying to lock!");
      }
      while (!started || runner.getState() != Thread.State.BLOCKED) {}
    }

    public synchronized void DoNotify() {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.NOTIFY);
    }

    public synchronized void DoNotifyAll() {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.NOTIFY_ALL);
    }

    public synchronized void DoTimedWaitLong() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.TIMED_LONG);
    }

    public synchronized void DoTimedWaitShort() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.TIMED_SHORT);
    }

    public synchronized void DoWait() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked");
      }
      setAction(Action.WAIT);
    }

    public synchronized void waitForActionToFinish() throws Exception {
      while (action.getReference() != Action.HOLD) {}
    }

    public synchronized void DoUnlock() throws Exception {
      if (!IsLocked()) {
        throw new Error("Not locked!");
      }
      setAction(Action.RELEASE);
      Thread run = runner;
      runner = null;
      while (held) {}
      run.join();
      action.set(Action.HOLD, 0);
    }
  }

  // A runnable that lets us know when a different thread is holding a monitor.
  // public static class ThreadPauser implements Runnable {
  //   private volatile boolean release;
  //   private volatile boolean pre_monitor_sync;
  //   private volatile boolean post_monitor_sync;
  //   private Object monitor;

  //   public ThreadPauser(Object monitor) {
  //     this.release = false;
  //     this.pre_monitor_sync = false;
  //     this.post_monitor_sync = false;
  //     this.monitor = monitor;
  //   }

  //   @Override
  //   public void run() {
  //     this.pre_monitor_sync = true;
  //     synchronized (monitor) {
  //       this.post_monitor_sync = true;
  //       while (!release) { }
  //       this.post_monitor_sync = false;
  //     }
  //     this.pre_monitor_sync = false;
  //   }

  //   public void waitForOtherThreadToStart() {
  //     while (!pre_monitor_sync) {}
  //   }

  //   public void waitForOtherThreadToAcquireMonitor() {
  //     while (!post_monitor_sync) { }
  //   }

  //   public void makeOtherThreadReleaseMonitor() {
  //     this.release = true;
  //     while (this.pre_monitor_sync) {}
  //   }
  // }

  // A lock with a toString.
  public static class NamedLock {
    public String name;
    public NamedLock(String name) { this.name = name; }
    public String toString() { return "NamedLock(\"" + name + "\")"; }
  }

  public static void handleMonitorEnter(Thread thd, Object lock) {
    System.out.println(thd.getName() + " contended-LOCKING " + lock);
  }

  public static void handleMonitorEntered(Thread thd, Object lock) {
    System.out.println(thd.getName() + " LOCKED " + lock);
  }
  public static void handleMonitorWait(Thread thd, Object lock, long timeout) {
    System.out.println(thd.getName() + " start-monitor-wait " + lock + " timeout: " + timeout);
  }

  public static void handleMonitorWaited(Thread thd, Object lock, boolean timed_out) {
    System.out.println(thd.getName() + " monitor-waited " + lock + " timed_out: " + timed_out);
  }

  public static void run() throws Exception {
    Monitors.setupMonitorEvents(
        Test1930.class,
        Test1930.class.getDeclaredMethod("handleMonitorEnter", Thread.class, Object.class),
        Test1930.class.getDeclaredMethod("handleMonitorEntered", Thread.class, Object.class),
        Test1930.class.getDeclaredMethod("handleMonitorWait",
          Thread.class, Object.class, Long.TYPE),
        Test1930.class.getDeclaredMethod("handleMonitorWaited",
          Thread.class, Object.class, Boolean.TYPE),
        NamedLock.class,
        null);

    // Just lock.
    testLock(new NamedLock("Lock testLock"));
    // Wait.
    testWait(new NamedLock("Lock testWait"));
    testTimedWait(new NamedLock("Lock testTimedWait"), 10000);
    testTimedWaitTimeout(new NamedLock("Lock testTimedWaitTimeout"), 100);
  }

  public static void testLock(NamedLock lk) throws Exception {
    LockController controller1 = new LockController(lk);
    LockController controller2 = new LockController(lk);
    // ThreadPauser pauser = new ThreadPauser(lk);
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    controller2.DoLock();
    if (controller2.isHeld()) {
      throw new Exception("c2 was able to gain lock while it was held by c1");
    }
    controller2.waitForContendedSleep();
    controller.DoUnlock();
    pauser.waitForOtherThreadToAcquireMonitor();
    pauser.makeOtherThreadReleaseMonitor();
    target.join();
  }

  public static void testWait(NamedLock lk) throws Exception {
    // LockController controller1 = new LockController(lk);
    // LockController controller2 = new LockController(lk);
    // controller1.DoLock();
    // controller1.DoWait();
    // ThreadPauser pauser = new ThreadPauser(lk);
    // Thread target = new Thread(pauser, "testWait thread");
    // TODO
  }
  public static void testTimedWait(NamedLock lk, long millis) throws Exception {
    // LockController controller = new LockController(lk);
    // ThreadPauser pauser = new ThreadPauser(lk);
    // Thread target = new Thread(pauser, "testTimedWait thread");
    // TODO
  }
  public static void testTimedWaitTimeout(NamedLock lk, long millis) throws Exception {
    // LockController controller = new LockController(lk);
    // ThreadPauser pauser = new ThreadPauser(lk);
    // Thread target = new Thread(pauser, "testTimedWaitTimeout thread");
    // TODO
  }
}
