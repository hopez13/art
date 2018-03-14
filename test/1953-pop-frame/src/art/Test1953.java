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

import java.util.function.Consumer;

public class Test1953 {
  public static void doNothing() {}

  public static interface TestSuspender {
    public void setup(Thread thr);
    public void waitForSuspend(Thread thr);
    public void cleanup(Thread thr);
  }

  public static interface ThreadRunnable { public void run(Thread thr); }
  public static TestSuspender makeSuspend(final ThreadRunnable setup, final ThreadRunnable clean) {
    return new TestSuspender() {
      public void setup(Thread thr) { setup.run(thr); }
      public void waitForSuspend(Thread thr) { Test1953.waitForSuspendHit(thr); }
      public void cleanup(Thread thr) { clean.run(thr); }
    };
  }

  public void runTestOn(Runnable testObj, ThreadRunnable su, ThreadRunnable cl) throws
      Exception {
    runTestOn(testObj, makeSuspend(su, cl));
  }

  public void runTestOn(Runnable testObj, TestSuspender su) throws Exception {
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

    // Do any final setup.
    preTest.accept(testObj);

    // Setup suspension method on the thread.
    su.setup(thr);

    // Let the other thread go.
    continue_latch.countDown();

    // Wait for the other thread to hit the breakpoint/watchpoint/whatever and suspend itself
    // (without re-entering java)
    su.waitForSuspend(thr);

    // Cleanup the breakpoint/watchpoint/etc.
    su.cleanup(thr);

    try {
      // Pop the frame.
      popFrame(thr);
    } catch (Exception e) {
      System.out.println("Failed to pop frame! " + e);
      e.printStackTrace(System.out);
    }

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

  public static class ClassLoadObject extends AbstractTestObject implements Runnable {
    public int cnt;

    public static final String[] CLASS_NAMES = new String[] {
      "Lart/Test1953$ClassLoadObject$A;",
      "Lart/Test1953$ClassLoadObject$B;",
      "Lart/Test1953$ClassLoadObject$C;",
      "Lart/Test1953$ClassLoadObject$D;",
      "Lart/Test1953$ClassLoadObject$E;",
      "Lart/Test1953$ClassLoadObject$F;",
      "Lart/Test1953$ClassLoadObject$G;",
      "Lart/Test1953$ClassLoadObject$H;",
      "Lart/Test1953$ClassLoadObject$I;",
      "Lart/Test1953$ClassLoadObject$J;",
      "Lart/Test1953$ClassLoadObject$K;"
    };

    private static int curClass = 0;

    private static class A { public static int foo; static { foo = 1; } }
    private static class B { public static int foo; static { foo = 2; } }
    private static class C { public static int foo; static { foo = 3; } }
    private static class D { public static int foo; static { foo = 4; } }
    private static class E { public static int foo; static { foo = 5; } }
    private static class F { public static int foo; static { foo = 6; } }
    private static class G { public static int foo; static { foo = 7; } }
    private static class H { public static int foo; static { foo = 8; } }
    private static class I { public static int foo; static { foo = 9; } }
    private static class J { public static int foo; static { foo = 10; } }
    private static class K { public static int foo; static { foo = 11; } }

    public ClassLoadObject() {
      super();
      cnt = 0;
    }

    public void calledFunction() {
      cnt++;
      if (curClass == 0) {
        System.out.println("A.foo == " + A.foo);
      } else if (curClass == 1) {
        System.out.println("B.foo == " + B.foo);
      } else if (curClass == 2) {
        System.out.println("C.foo == " + C.foo);
      } else if (curClass == 3) {
        System.out.println("D.foo == " + D.foo);
      } else if (curClass == 4) {
        System.out.println("E.foo == " + E.foo);
      } else if (curClass == 5) {
        System.out.println("F.foo == " + F.foo);
      } else if (curClass == 6) {
        System.out.println("G.foo == " + G.foo);
      } else if (curClass == 7) {
        System.out.println("H.foo == " + H.foo);
      } else if (curClass == 8) {
        System.out.println("I.foo == " + I.foo);
      } else if (curClass == 9) {
        System.out.println("J.foo == " + J.foo);
      } else if (curClass == 10) {
        System.out.println("K.foo == " + K.foo);
      }
      curClass++;
    }

    public String toString() {
      return "ClassLoadObject { cnt: " + cnt + ", curClass: " + curClass + "}";
    }
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

  public static class SynchronizedFunctionTestObject extends AbstractTestObject implements Runnable {
    public int cnt;

    public SynchronizedFunctionTestObject() {
      super();
      cnt = 0;
    }

    public synchronized void calledFunction() {
      cnt++;               // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      doNothing();         // line +2
    }

    public String toString() {
      return "SynchronizedFunctionTestObject { cnt: " + cnt + " }";
    }
  }
  public static class SynchronizedTestObject extends AbstractTestObject implements Runnable {
    public int cnt;
    public final Object lock;

    public SynchronizedTestObject() {
      super();
      cnt = 0;
      lock = new Object();
    }

    public void calledFunction() {
      synchronized (lock) {  // line +0
        cnt++;               // line +1
        // We put a breakpoint here and PopFrame when we are at it.
        doNothing();         // line +3
      }
    }

    public String toString() {
      return "SynchronizedTestObject { cnt: " + cnt + " }";
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
        System.out.println(e.getClass().getName() + " caught in called function.");
      }
    }

    public void doThrow() {
      throw new TestError();
    }

    public String toString() {
      return "ExceptionCatchTestObject { cnt: " + cnt + " }";
    }
  }
  public static class ExceptionThrowFarTestObject implements Runnable {
    public static class TestError extends Error {}

    public int cnt;
    public final boolean catchInCalled;
    public ExceptionThrowFarTestObject(boolean catchInCalled) {
      super();
      cnt = 0;
      this.catchInCalled = catchInCalled;
    }

    public void run() {
      try {
        callingFunction();
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " thrown and caught!");
      }
    }

    public void callingFunction() {
      calledFunction();
    }
    public void calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println(e.getClass().getName() + " caught in same function.");
        }
      } else {
        throw new TestError(); // We put a watch here.
      }
    }

    public String toString() {
      return "ExceptionThrowFarTestObject { cnt: " + cnt + " }";
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
        System.out.println(e.getClass().getName() + " thrown and caught!");
      }
    }

    public void calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println(e.getClass().getName() + " caught in same function.");
        }
      } else {
        throw new TestError(); // We put a watch here.
      }
    }

    public String toString() {
      return "ExceptionThrowTestObject { cnt: " + cnt + " }";
    }
  }

  public static class SuspendSuddenlyObject implements Runnable {
    public volatile boolean stop_spinning = false;
    public volatile boolean is_spinning = false;
    public int cnt = 0;

    public void run() {
      calledFunction();
    }

    public void calledFunction() {
      cnt++;
      while (!stop_spinning) {
        is_spinning = true;
      }
    }

    public String toString() {
      return "SuspendSuddenlyObject { cnt: " + cnt + " }";
    }
  }

  public static void run() throws Exception {
    new Test1953((x)-> {}).runTests();
  }

  public Test1953(Consumer<Object> preTest) {
    this.preTest = preTest;
  }

  private Consumer<Object> preTest;

  public void runTests() throws Exception {
    setupTest();

    final Method calledFunction = StandardTestObject.class.getDeclaredMethod("calledFunction");
    final Method doNothingMethod = Test1953.class.getDeclaredMethod("doNothing");
    // Add a breakpoint on the second line after the start of the function
    final int line = Breakpoint.locationToLine(calledFunction, 0) + 2;
    final long loc = Breakpoint.lineToLocation(calledFunction, line);
    System.out.println("Test stopped using breakpoint");
    runTestOn(new StandardTestObject(),
        (thr) -> { setupSuspendBreakpointFor(calledFunction, loc, thr); },
        Test1953::clearSuspendBreakpointFor);

    final Method syncFunctionCalledFunction =
        SynchronizedFunctionTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    // Annoyingly r8 generally has the first instruction (a monitor enter) not be marked as being
    // on any line but javac has it marked as being on the first line of the function. Just use the
    // second entry on the line-number table to get the breakpoint. This should be good for both.
    final long syncFunctionLoc =
        Breakpoint.getLineNumberTable(syncFunctionCalledFunction)[1].location;
    System.out.println("Test stopped using breakpoint with declared synchronized function");
    runTestOn(new SynchronizedFunctionTestObject(),
        (thr) -> { setupSuspendBreakpointFor(syncFunctionCalledFunction, syncFunctionLoc, thr); },
        Test1953::clearSuspendBreakpointFor);

    final Method syncCalledFunction =
        SynchronizedTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    final int syncLine = Breakpoint.locationToLine(syncCalledFunction, 0) + 3;
    final long syncLoc = Breakpoint.lineToLocation(syncCalledFunction, syncLine);
    System.out.println("Test stopped using breakpoint with synchronized block");
    runTestOn(new SynchronizedTestObject(),
        (thr) -> { setupSuspendBreakpointFor(syncCalledFunction, syncLoc, thr); },
        Test1953::clearSuspendBreakpointFor);

    System.out.println("Test stopped on single step");
    runTestOn(new StandardTestObject(),
        (thr) -> setupSuspendSingleStepAt(calledFunction, loc, thr),
        Test1953::clearSuspendSingleStepFor);

    final Field target_field = FieldBasedTestObject.class.getDeclaredField("TARGET_FIELD");
    System.out.println("Test stopped on field access");
    runTestOn(new FieldBasedTestObject(),
        (thr) -> setupFieldSuspendFor(FieldBasedTestObject.class, target_field, true, thr),
        Test1953::clearFieldSuspendFor);

    System.out.println("Test stopped on field modification");
    runTestOn(new FieldBasedTestObject(),
        (thr) -> setupFieldSuspendFor(FieldBasedTestObject.class, target_field, false, thr),
        Test1953::clearFieldSuspendFor);

    System.out.println("Test stopped during Method Exit of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(doNothingMethod, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);

    // NB We need another test to make sure the MethodEntered event is triggered twice.
    System.out.println("Test stopped during Method Enter of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(doNothingMethod, /*enter*/ true, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Exit of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(calledFunction, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Enter of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(calledFunction, /*enter*/ true, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during notifyFramePop without exception on pop of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(1, doNothingMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    System.out.println("Test stopped during notifyFramePop without exception on pop of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(0, doNothingMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    final Method exceptionThrowCalledMethod =
        ExceptionThrowTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during notifyFramePop with exception on pop of calledFunction");
    runTestOn(new ExceptionThrowTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(0, exceptionThrowCalledMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    final Method exceptionCatchThrowMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("doThrow");
    System.out.println("Test stopped during notifyFramePop with exception on pop of doThrow");
    runTestOn(new ExceptionCatchTestObject(),
        (thr) -> setupSuspendPopFrameEvent(0, exceptionCatchThrowMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    System.out.println("Test stopped during ExceptionCatch event of calledFunction " +
        "(catch in called function, throw in called function)");
    runTestOn(new ExceptionThrowTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ true, thr),
        Test1953::clearSuspendExceptionEvent);

    final Method exceptionCatchCalledMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during ExceptionCatch event of calledFunction " +
        "(catch in called function, throw in subroutine)");
    runTestOn(new ExceptionCatchTestObject(),
        (thr) -> setupSuspendExceptionEvent(exceptionCatchCalledMethod, /*catch*/ true, thr),
        Test1953::clearSuspendExceptionEvent);

     System.out.println("Test stopped during Exception event of calledFunction " +
         "(catch in calling function)");
     runTestOn(new ExceptionThrowTestObject(false),
         (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ false, thr),
         Test1953::clearSuspendExceptionEvent);

     System.out.println("Test stopped during Exception event of calledFunction " +
         "(catch in called function)");
     runTestOn(new ExceptionThrowTestObject(true),
         (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ false, thr),
         Test1953::clearSuspendExceptionEvent);

     final Method exceptionThrowFarCalledMethod =
         ExceptionThrowFarTestObject.class.getDeclaredMethod("calledFunction");
     System.out.println("Test stopped during Exception event of calledFunction " +
         "(catch in parent of calling function)");
     runTestOn(new ExceptionThrowFarTestObject(false),
         (thr) -> setupSuspendExceptionEvent(exceptionThrowFarCalledMethod, /*catch*/ false, thr),
         Test1953::clearSuspendExceptionEvent);

     System.out.println("Test stopped during Exception event of calledFunction " +
         "(catch in called function)");
     runTestOn(new ExceptionThrowFarTestObject(true),
         (thr) -> setupSuspendExceptionEvent(exceptionThrowFarCalledMethod, /*catch*/ false, thr),
         Test1953::clearSuspendExceptionEvent);

     System.out.println("Test stopped during a ClassLoad event.");
     runTestOn(new ClassLoadObject(),
         (thr) -> setupSuspendClassEvent(EVENT_TYPE_CLASS_LOAD, ClassLoadObject.CLASS_NAMES, thr),
         Test1953::clearSuspendClassEvent);

     System.out.println("Test stopped during a ClassPrepare event.");
     runTestOn(new ClassLoadObject(),
         (thr) -> setupSuspendClassEvent(EVENT_TYPE_CLASS_PREPARE,
                                         ClassLoadObject.CLASS_NAMES,
                                         thr),
         (thr) -> {
           clearSuspendClassEvent(thr);
           // for (StackTrace.StackFrameData data : StackTrace.GetStackTrace(thr)) {
           //   System.out.println(data);
           // }
         });

     System.out.println("Test stopped during random Suspend.");
     final SuspendSuddenlyObject sso = new SuspendSuddenlyObject();
     runTestOn(
         sso,
         new TestSuspender() {
           public void setup(Thread thr) { }
           public void waitForSuspend(Thread thr) {
             while (!sso.is_spinning) {}
             Suspension.suspend(thr);
           }
           public void cleanup(Thread thr) {
             sso.stop_spinning = true;
           }
         });
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

  public static native void setupSuspendPopFrameEvent(
      int offset, Executable breakpointFunction, Thread thr);
  public static native void clearSuspendPopFrameEvent(Thread thr);

  public static final int EVENT_TYPE_CLASS_LOAD = 55;
  public static final int EVENT_TYPE_CLASS_PREPARE = 56;
  public static native void setupSuspendClassEvent(
      int eventType, String[] interesting_names, Thread thr);
  public static native void clearSuspendClassEvent(Thread thr);

  public static native void waitForSuspendHit(Thread thr);
}
