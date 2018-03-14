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

import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

import java.time.Duration;

import java.util.concurrent.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.Random;
import java.util.Stack;
import java.util.Vector;

import java.util.function.Supplier;

public class Test1950 {
  public static void doNothing() {}

  public static interface TestSuspender {
    public void setup(Thread thr);
    public void cleanup(Thread thr);
  }

  public static interface ThreadRunnable { public void run(Thread thr); }
  public static TestSuspender makeSuspend(final ThreadRunnable setup, final ThreadRunnable clean) {
    return new TestSuspender() {
      public void setup(Thread thr) { setup.run(thr); }
      public void cleanup(Thread thr) { clean.run(thr); }
    };
  }

  public static void runTestOn(Runnable testObj, ThreadRunnable su, ThreadRunnable cl) throws
      Exception {
    runTestOn(testObj, makeSuspend(su, cl));
  }

  public static void runTestOn(Runnable testObj, TestSuspender su) throws Exception {
    System.out.println("Single call with PopFrame on " + testObj);
    final CountDownLatch continue_latch = new CountDownLatch(1);
    final CountDownLatch startup_latch = new CountDownLatch(1);
    Runnable await = () -> {
      try {
        startup_latch.countDown();
        continue_latch.await();
      } catch (Exception e) {
        throw new Error("Failed to await latch", e);
      }
    };
    Thread thr = new Thread(() -> { await.run(); testObj.run(); });
    thr.start();

    // Wait until the other thread is started.
    startup_latch.await();

    // Setup suspension method on the thread.
    su.setup(thr);

    // Let the other thread go.
    continue_latch.countDown();

    // Wait for the other thread to hit the breakpoint/watchpoint/whatever and suspend itself
    // (without re-entering java)
    waitForSuspendHit(thr);

    // Cleanup the breakpoint/watchpoint/etc.
    su.cleanup(thr);

    // Pop the frame.
    popFrame(thr);

    // Start the other thread going again.
    Suspension.resume(thr);

    // Wait for the other thread to finish.
    thr.join();

    // See how many times calledFunction was called.
    System.out.println("result is " + testObj);
  }

  public static abstract class AbstractTestObject implements Runnable {
    public void run() {
      // This function should be re-executed by the popFrame.
      calledFunction();
    }

    public abstract void calledFunction();
  }

  public static class FieldBasedTestObject extends AbstractTestObject implements Runnable {
    public int cnt;
    public int TARGET_FIELD;
    public FieldBasedTestObject() {
      super();
      cnt = 0;
      TARGET_FIELD = 0;
    }

    public void calledFunction() {
      cnt++;
      // We put a watchpoint here and PopFrame when we are at it.
      TARGET_FIELD += 10;
      if (cnt == 1) { System.out.println("FAILED: No pop on first call!"); }
    }

    public String toString() {
      return "FieldBasedTestObject { cnt: " + cnt + ", TARGET_FIELD: " + TARGET_FIELD + " }";
    }
  }

  public static class StandardTestObject extends AbstractTestObject implements Runnable {
    public int cnt;
    public final boolean check;

    public StandardTestObject(boolean check) {
      super();
      cnt = 0;
      this.check = check;
    }

    public StandardTestObject() {
      this(true);
    }

    public void calledFunction() {
      cnt++;       // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      doNothing(); // line +2
      if (check && cnt == 1) { System.out.println("FAILED: No pop on first call!"); }
    }

    public String toString() {
      return "StandardTestObject { cnt: " + cnt + " }";
    }
  }

  public static class ExceptionCatchTestObject extends AbstractTestObject implements Runnable {
    public static class TestError extends Error {}

    public int cnt;
    public ExceptionCatchTestObject() {
      super();
      cnt = 0;
    }

    public void calledFunction() {
      cnt++;
      try {
        doThrow();
      } catch (TestError e) {
        System.out.println("TestError caught in called function.");
      }
    }

    public void doThrow() {
      throw new TestError();
    }

    public String toString() {
      return "ExceptionCatchTestObject { cnt: " + cnt + " }";
    }
  }
  public static class ExceptionThrowTestObject implements Runnable {
    public static class TestError extends Error {}

    public int cnt;
    public final boolean catchInCalled;
    public ExceptionThrowTestObject(boolean catchInCalled) {
      super();
      cnt = 0;
      this.catchInCalled = catchInCalled;
    }

    public void run() {
      try {
        calledFunction();
      } catch (TestError e) {
        System.out.println("TestError thrown and caught!");
      }
    }

    public void calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println("TestError caught in same function.");
        }
      } else {
        throw new TestError(); // We put a watch here.
      }
    }

    public String toString() {
      return "ExceptionThrowTestObject { cnt: " + cnt + " }";
    }
  }

  public static void run() throws Exception {
    setupTest();

    final Method calledFunction = StandardTestObject.class.getDeclaredMethod("calledFunction");
    final Method doNothingMethod = Test1950.class.getDeclaredMethod("doNothing");
    // Add a breakpoint on the second line after the start of the function
    final int line = Breakpoint.locationToLine(calledFunction, 0) + 2;
    final long loc = Breakpoint.lineToLocation(calledFunction, line);
    System.out.println("Test stopped using breakpoint");
    runTestOn(new StandardTestObject(),
        (thr) -> { setupSuspendBreakpointFor(calledFunction, loc, thr); },
        Test1950::clearSuspendBreakpointFor);

    System.out.println("Test stopped on single step");
    runTestOn(new StandardTestObject(),
        (thr) -> { setupSuspendSingleStepAt(calledFunction, loc, thr); },
        Test1950::clearSuspendSingleStepFor);

    final Field target_field = FieldBasedTestObject.class.getDeclaredField("TARGET_FIELD");
    System.out.println("Test stopped on field access");
    runTestOn(new FieldBasedTestObject(),
        (thr) -> { setupFieldSuspendFor(FieldBasedTestObject.class, target_field, true, thr); },
        Test1950::clearFieldSuspendFor);

    System.out.println("Test stopped on field modification");
    runTestOn(new FieldBasedTestObject(),
        (thr) -> { setupFieldSuspendFor(FieldBasedTestObject.class, target_field, false, thr); },
        Test1950::clearFieldSuspendFor);

    System.out.println("Test stopped during Method Exit of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> { setupSuspendMethodEvent(doNothingMethod, /*enter*/ false, thr); },
        Test1950::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Enter of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> { setupSuspendMethodEvent(doNothingMethod, /*enter*/ true, thr); },
        Test1950::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Exit of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> { setupSuspendMethodEvent(calledFunction, /*enter*/ false, thr); },
        Test1950::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Enter of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> { setupSuspendMethodEvent(calledFunction, /*enter*/ true, thr); },
        Test1950::clearSuspendMethodEvent);

    // TODO Testing NotifyFramePop would be nice but it's hard to setup and the path is the same as
    // MethodExit for ART.

    final Method exceptionThrowCalledMethod =
        ExceptionThrowTestObject.class.getDeclaredMethod("calledFunction");

    System.out.println("Test stopped during Exception event of calledFunction " +
        "(catch in calling function)");
    runTestOn(new ExceptionThrowTestObject(false),
        (thr) -> { setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ false, thr); },
        Test1950::clearSuspendExceptionEvent);

    System.out.println("Test stopped during Exception event of calledFunction " +
        "(catch in called function)");
    runTestOn(new ExceptionThrowTestObject(true),
        (thr) -> { setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ false, thr); },
        Test1950::clearSuspendExceptionEvent);

    System.out.println("Test stopped during ExceptionCatch event of calledFunction " +
        "(catch in called function, throw in called function)");
    runTestOn(new ExceptionThrowTestObject(true),
        (thr) -> { setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ true, thr); },
        Test1950::clearSuspendExceptionEvent);

    final Method exceptionCatchCalledMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during ExceptionCatch event of calledFunction " +
        "(catch in called function, throw in subroutine)");
    runTestOn(new ExceptionCatchTestObject(),
        (thr) -> { setupSuspendExceptionEvent(exceptionCatchCalledMethod, /*catch*/ true, thr); },
        Test1950::clearSuspendExceptionEvent);
  }

  public static native void setupTest();
  public static native void popFrame(Thread thr);

  public static native void setupSuspendBreakpointFor(Executable meth, long loc, Thread thr);
  public static native void clearSuspendBreakpointFor(Thread thr);

  public static native void setupSuspendSingleStepAt(Executable meth, long loc, Thread thr);
  public static native void clearSuspendSingleStepFor(Thread thr);

  public static native void setupFieldSuspendFor(Class klass, Field f, boolean access, Thread thr);
  public static native void clearFieldSuspendFor(Thread thr);

  public static native void setupSuspendMethodEvent(Executable meth, boolean enter, Thread thr);
  public static native void clearSuspendMethodEvent(Thread thr);

  public static native void setupSuspendExceptionEvent(
      Executable meth, boolean is_catch, Thread thr);
  public static native void clearSuspendExceptionEvent(Thread thr);

  public static native void waitForSuspendHit(Thread thr);
}
