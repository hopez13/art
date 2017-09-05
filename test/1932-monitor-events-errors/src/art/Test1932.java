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

public class Test1932 {
  public static final boolean PRINT_FULL_STACK_TRACE = false;
  public static void printStackTrace(Throwable t) {
    System.out.println("Caught exception: " + t);
    for (Throwable c = t.getCause(); c != null; c = c.getCause()) {
      System.out.println("\tCaused by: " +
          (Test1932.class.getPackage().equals(c.getClass().getPackage())
           ? c.toString() : c.getClass().toString()));
    }
    if (PRINT_FULL_STACK_TRACE) {
      t.printStackTrace();
    }
  }

  public static interface MonitorHandler {
    public default void handleMonitorEnter(Thread thd, Object lock) {}
    public default void handleMonitorEntered(Thread thd, Object lock) {}
    public default void handleMonitorWait(Thread thd, Object lock, long timeout) {}
    public default void handleMonitorWaited(Thread thd, Object lock, boolean timed_out) {}
  }

  public static volatile MonitorHandler HANDLER = null;

  public static void handleMonitorEnter(Thread thd, Object lock) {
    System.out.println(thd.getName() + " contended-LOCKING " + lock);
    if (HANDLER != null) {
      HANDLER.handleMonitorEnter(thd, lock);
    }
  }

  public static void handleMonitorEntered(Thread thd, Object lock) {
    System.out.println(thd.getName() + " LOCKED " + lock);
    if (HANDLER != null) {
      HANDLER.handleMonitorEntered(thd, lock);
    }
  }
  public static void handleMonitorWait(Thread thd, Object lock, long timeout) {
    System.out.println(thd.getName() + " start-monitor-wait " + lock + " timeout: " + timeout);
    if (HANDLER != null) {
      HANDLER.handleMonitorWait(thd, lock, timeout);
    }
  }

  public static void handleMonitorWaited(Thread thd, Object lock, boolean timed_out) {
    System.out.println(thd.getName() + " monitor-waited " + lock + " timed_out: " + timed_out);
    if (HANDLER != null) {
      HANDLER.handleMonitorWaited(thd, lock, timed_out);
    }
  }

  public static void run() throws Exception {
    Monitors.setupMonitorEvents(
        Test1932.class,
        Test1932.class.getDeclaredMethod("handleMonitorEnter", Thread.class, Object.class),
        Test1932.class.getDeclaredMethod("handleMonitorEntered", Thread.class, Object.class),
        Test1932.class.getDeclaredMethod("handleMonitorWait",
          Thread.class, Object.class, Long.TYPE),
        Test1932.class.getDeclaredMethod("handleMonitorWaited",
          Thread.class, Object.class, Boolean.TYPE),
        Monitors.NamedLock.class,
        null);

    System.out.println("Testing contended locking where lock is released before callback ends.");
    testLockUncontend(new Monitors.NamedLock("Lock testLockUncontend"));

    System.out.println("Testing throwing exceptions in monitor_enter");
    testLockThrowEnter(new Monitors.NamedLock("Lock testLockThrowEnter"));

    System.out.println("Testing throwing exceptions in monitor_entered");
    testLockThrowEntered(new Monitors.NamedLock("Lock testLockThrowEntered"));

    System.out.println("Testing throwing exceptions in both monitorEnter & MonitorEntered");
    testLockThrowBoth(new Monitors.NamedLock("Lock testLockThrowBoth"));

    System.out.println("Testing throwing exception in MonitorWait event");
    testThrowWait(new Monitors.NamedLock("Lock testThrowWait"));

    System.out.println("Testing throwing exception in MonitorWait event with illegal aruments");
    testThrowIllegalWait(new Monitors.NamedLock("Lock testThrowIllegalWait"));

    System.out.println("TODO Other ones.");
    // throw new Error("Not done.");
  }

  public static void testThrowWait(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      public void handleMonitorWait(Thread thd, Object l, long timeout) {
        System.out.println("Throwing exception in MonitorWait");
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller1.DoWait();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
      controller1.DoCleanup();
    }
  }

  public static void testThrowIllegalWait(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk, -100000);
    HANDLER = new MonitorHandler() {
      public void handleMonitorWait(Thread thd, Object l, long timeout) {
        System.out.println("Throwing exception in MonitorWait timeout = " + timeout);
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
    };
    try {
      controller1.DoLock();
      controller1.waitForLockToBeHeld();
      controller1.DoTimedWait();
      controller1.waitForLockToBeHeld();
      controller1.DoUnlock();
      System.out.println("No Exception thrown!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
      controller1.DoCleanup();
    }
  }

  public static void testLockUncontend(final Monitors.NamedLock lk) throws Exception {
    final Monitors.LockController controller1 = new Monitors.LockController(lk);
    final Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      public void handleMonitorEnter(Thread thd, Object lock) {
        if (controller1.IsLocked()) {
          System.out.println("Releasing " + lk + " during monitorEnter event.");
          try {
            controller1.DoUnlock();
          } catch (Exception e) {
            throw new Error("Unable to unlock controller1", e);
          }
        } else {
          throw new Error("controller1 does not seem to hold the lock!");
        }
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    // This will call handleMonitorEnter but will release during the callback.
    controller2.DoLock();
    controller2.waitForLockToBeHeld();
    if (controller1.IsLocked()) {
      throw new Error("controller1 still holds the lock somehow!");
    }
    controller2.DoUnlock();
  }

  public static void testLockThrowEnter(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      public void handleMonitorEnter(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEnter");
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller2.DoLock();
      controller2.waitForContendedSleep();
      controller1.DoUnlock();
      controller2.waitForLockToBeHeld();
      controller2.DoUnlock();
      System.out.println("Did not get an exception!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
      controller2.DoCleanup();
    }
  }

  public static void testLockThrowEntered(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      public void handleMonitorEntered(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEntered");
        throw new Monitors.TestException("throwing exception during monitorEntered of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller2.DoLock();
      controller2.waitForContendedSleep();
      controller1.DoUnlock();
      controller2.waitForLockToBeHeld();
      controller2.DoUnlock();
      System.out.println("Did not get an exception!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
      controller2.DoCleanup();
    }
  }

  public static void testLockThrowBoth(Monitors.NamedLock lk) throws Exception {
    Monitors.LockController controller1 = new Monitors.LockController(lk);
    Monitors.LockController controller2 = new Monitors.LockController(lk);
    HANDLER = new MonitorHandler() {
      public void handleMonitorEnter(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEnter");
        throw new Monitors.TestException("throwing exception during monitorEnter of " + l);
      }
      public void handleMonitorEntered(Thread t, Object l) {
        System.out.println("Throwing exception in MonitorEntered");
        throw new Monitors.TestException("throwing exception during monitorEntered of " + l);
      }
    };
    controller1.DoLock();
    controller1.waitForLockToBeHeld();
    try {
      controller2.DoLock();
      controller2.waitForContendedSleep();
      controller1.DoUnlock();
      controller2.waitForLockToBeHeld();
      controller2.DoUnlock();
      System.out.println("Did not get an exception!");
    } catch (Monitors.TestException e) {
      printStackTrace(e);
      controller2.DoCleanup();
    }
  }
}
