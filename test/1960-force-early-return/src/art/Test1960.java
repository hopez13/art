/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

package art;

import java.io.*;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.concurrent.CountDownLatch;
import java.util.function.Consumer;
import java.util.function.Supplier;

public class Test1960 {
  public static final boolean PRINT_STACK_TRACE = false;
  public static long OVERRIDE_ID = 0;

  public static final class OverridenReturnValue {
    public final Thread target;
    public final Thread.State state;
    public final StackTraceElement stack[];
    public final long id;

    public OverridenReturnValue(Thread thr) {
      target = thr;
      state = thr.getState();
      stack = thr.getStackTrace();
      id = OVERRIDE_ID++;
    }

    public String toString() {
      String stackTrace =
          PRINT_STACK_TRACE
              ? ",\n\tthread: "
                  + target.toString()
                  + ",\n\tstate: "
                  + state
                  + ",\n\tstack:\n"
                  + safeDumpStackTrace(stack, "\t\t")
                  + ",\n\t"
              : "";
      return "OverridenReturnValue { id: " + id + stackTrace + " }";
    }
  }

  // Returns a value to be used for the return value of the given thread.
  public static Object getOverridenReturnValue(Thread thr) {
    return new OverridenReturnValue(thr);
  }

  public final boolean canRunClassLoadTests;

  public static void doNothing() {}

  public interface TestRunnable extends Runnable {
    public Object getReturnValue();
  }

  public static interface TestSuspender {
    public void setup(Thread thr);

    public void waitForSuspend(Thread thr);

    public void cleanup(Thread thr);

    public void performForceReturn(Thread thr);
  }

  public static interface ThreadRunnable {
    public void run(Thread thr);
  }

  public static TestSuspender makeSuspend(final ThreadRunnable setup, final ThreadRunnable clean) {
    return new TestSuspender() {
      public void setup(Thread thr) {
        setup.run(thr);
      }

      public void waitForSuspend(Thread thr) {
        Test1960.waitForSuspendHit(thr);
      }

      public void cleanup(Thread thr) {
        clean.run(thr);
      }

      public void performForceReturn(Thread thr) {
        Object ret = getOverridenReturnValue(thr);
        System.out.println("Will force return of " + ret);
        NonStandardExit.forceEarlyReturn(thr, ret);
      }
    };
  }

  public void runTestOn(Supplier<TestRunnable> testObj, ThreadRunnable su, ThreadRunnable cl)
      throws Exception {
    runTestOn(testObj, makeSuspend(su, cl));
  }

  private static void SafePrintStackTrace(StackTraceElement st[]) {
    System.out.println(safeDumpStackTrace(st, "\t"));
  }

  private static String safeDumpStackTrace(StackTraceElement st[], String prefix) {
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    PrintStream os = new PrintStream(baos);
    for (StackTraceElement e : st) {
      os.println(
          prefix
              + e.getClassName()
              + "."
              + e.getMethodName()
              + "("
              + (e.isNativeMethod() ? "Native Method" : e.getFileName())
              + ")");
      if (e.getClassName().equals("art.Test1960") && e.getMethodName().equals("runTests")) {
        os.println(prefix + "<Additional frames hidden>");
        break;
      }
    }
    os.flush();
    return baos.toString();
  }

  static long ID_COUNTER = 0;

  public TestRunnable Id(final TestRunnable tr) {
    final long my_id = ID_COUNTER++;
    return new TestRunnable() {
      public void run() {
        tr.run();
      }

      public Object getReturnValue() {
        return tr.getReturnValue();
      }

      public String toString() {
        return "(ID: " + my_id + ") " + tr.toString();
      }
    };
  }

  public static long THREAD_COUNT = 0;

  public Thread mkThread(Runnable r) {
    Thread t = new Thread(r, "Test1960 target thread - " + THREAD_COUNT++);
    t.setUncaughtExceptionHandler(
        (thr, e) -> {
          System.out.println(
              "Uncaught exception in thread "
                  + thr
                  + " - "
                  + e.getClass().getName()
                  + ": "
                  + e.getLocalizedMessage());
          SafePrintStackTrace(e.getStackTrace());
        });
    return t;
  }

  public void runTestOn(Supplier<TestRunnable> testObjGen, TestSuspender su) throws Exception {
    TestRunnable normal_run = Id(testObjGen.get());
    try {
      System.out.println("NORMAL RUN: Single call with no interference on " + normal_run);
      Thread normal_thread = mkThread(normal_run);
      normal_thread.start();
      normal_thread.join();
      System.out.println(
          "NORMAL RUN: result for " + normal_run + " is " + normal_run.getReturnValue());
    } catch (Exception e) {
      System.out.println("NORMAL RUN: Ended with exception for " + normal_run + "!");
      e.printStackTrace(System.out);
    }

    TestRunnable testObj = Id(testObjGen.get());
    System.out.println("Single call with force-early-return on " + testObj);
    final CountDownLatch continue_latch = new CountDownLatch(1);
    final CountDownLatch startup_latch = new CountDownLatch(1);
    Runnable await =
        () -> {
          try {
            startup_latch.countDown();
            continue_latch.await();
          } catch (Exception e) {
            throw new Error("Failed to await latch", e);
          }
        };
    Thread thr =
        mkThread(
            () -> {
              await.run();
              testObj.run();
            });
    thr.start();

    // Wait until the other thread is started.
    startup_latch.await();

    // Do any final setup.
    preTest.accept(testObj);

    // Setup suspension method on the thread.
    su.setup(thr);

    // Let the other thread go.
    continue_latch.countDown();

    // Wait for the other thread to hit the breakpoint/watchpoint/whatever and
    // suspend itself
    // (without re-entering java)
    su.waitForSuspend(thr);

    // Cleanup the breakpoint/watchpoint/etc.
    su.cleanup(thr);

    try {
      // Pop the frame.
      su.performForceReturn(thr);
    } catch (Exception e) {
      System.out.println("Failed to force-return due to " + e);
      SafePrintStackTrace(e.getStackTrace());
    }

    // Start the other thread going again.
    Suspension.resume(thr);

    // Wait for the other thread to finish.
    thr.join();

    // See how many times calledFunction was called.
    System.out.println("result for " + testObj + " is " + testObj.getReturnValue());
  }

  public abstract static class AbstractTestObject implements TestRunnable {
    private Object resultObject;

    public AbstractTestObject() {
      resultObject = null;
    }

    public Object getReturnValue() {
      return resultObject;
    }

    public void run() {
      // This function should have it's return-value replaced by force-early-return.
      resultObject = calledFunction();
    }

    public abstract Object calledFunction();
  }

  public static class IntContainer {
    private final int value;

    public IntContainer(int i) {
      value = i;
    }

    public String toString() {
      return "IntContainer { value: " + value + " }";
    }
  }

  public static class FieldBasedTestObject extends AbstractTestObject implements Runnable {
    public int TARGET_FIELD;

    public FieldBasedTestObject() {
      super();
      TARGET_FIELD = 0;
    }

    public Object calledFunction() {
      // We put a watchpoint here and force-early-return when we are at it.
      TARGET_FIELD += 10;
      return new IntContainer(TARGET_FIELD);
    }

    public String toString() {
      return "FieldBasedTestObject { TARGET_FIELD: " + TARGET_FIELD + " }";
    }
  }

  public static class StandardTestObject extends AbstractTestObject implements Runnable {
    public int cnt;

    public StandardTestObject() {
      super();
      cnt = 0;
    }

    public Object calledFunction() {
      cnt++; // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      Object result = new IntContainer(cnt); // line +2
      cnt++; // line +3
      return result; // line +4
    }

    public String toString() {
      return "StandardTestObject { cnt: " + cnt + " }";
    }
  }

  public static class SynchronizedFunctionTestObject extends AbstractTestObject
      implements Runnable {
    public int cnt;

    public SynchronizedFunctionTestObject() {
      super();
      cnt = 0;
    }

    public synchronized Object calledFunction() {
      cnt++; // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      Object result = new IntContainer(cnt); // line +2
      cnt++; // line +3
      return result;
    }

    public String toString() {
      return "SynchronizedFunctionTestObject { cnt: " + cnt + " }";
    }
  }

  public static class SynchronizedTestObject extends AbstractTestObject implements Runnable {
    public final Object lock;
    public int cnt;

    public SynchronizedTestObject() {
      super();
      lock = new Object();
      cnt = 0;
    }

    public Object calledFunction() {
      synchronized (lock) { // line +0
        cnt++; // line +1
        // We put a breakpoint here and PopFrame when we are at it.
        Object result = new IntContainer(cnt); // line +3
        cnt++; // line +4
        return result; // line +5
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

    public Object calledFunction() {
      cnt++;
      Object result = new IntContainer(cnt);
      try {
        doThrow();
        cnt += 100;
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " caught in called function.");
        cnt++;
      }
      return result;
    }

    public Object doThrow() {
      throw new TestError();
    }

    public String toString() {
      return "ExceptionCatchTestObject { cnt: " + cnt + " }";
    }
  }

  public static class ExceptionThrowFarTestObject implements TestRunnable {
    public static class TestError extends Error {}

    public int cnt;
    public int baseCallCnt;
    public final boolean catchInCalled;
    public Object result;

    public ExceptionThrowFarTestObject(boolean catchInCalled) {
      super();
      cnt = 0;
      baseCallCnt = 0;
      this.catchInCalled = catchInCalled;
    }

    public void run() {
      baseCallCnt++;
      try {
        result = callingFunction();
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " thrown and caught!");
      }
      baseCallCnt++;
    }

    public Object callingFunction() {
      return calledFunction();
    }

    public Object calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          cnt += 100;
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println(e.getClass().getName() + " caught in same function.");
          Object result = new IntContainer(cnt);
          cnt += 10;
          return result;
        }
      } else {
        cnt++;
        throw new TestError(); // We put a watch here.
      }
    }

    public String toString() {
      return "ExceptionThrowFarTestObject { cnt: " + cnt + ", baseCnt: " + baseCallCnt + " }";
    }

    @Override
    public Object getReturnValue() {
      return result;
    }
  }

  public static class ExceptionOnceObject extends AbstractTestObject {
    public static final class TestError extends Error {}

    public int cnt;
    public final boolean throwInSub;

    public ExceptionOnceObject(boolean throwInSub) {
      super();
      cnt = 0;
      this.throwInSub = throwInSub;
    }

    public Object calledFunction() {
      cnt++;
      if (cnt == 1) {
        if (throwInSub) {
          return doThrow();
        } else {
          throw new TestError();
        }
      }
      return new IntContainer(cnt++);
    }

    public Object doThrow() {
      throw new TestError();
    }

    public String toString() {
      return "ExceptionOnceObject { cnt: " + cnt + ", throwInSub: " + throwInSub + " }";
    }
  }

  public static class ExceptionThrowTestObject implements TestRunnable {
    public static class TestError extends Error {}

    public Object getReturnValue() {
      return result;
    }

    public int cnt;
    public int baseCallCnt;
    public final boolean catchInCalled;
    public Object result;

    public ExceptionThrowTestObject(boolean catchInCalled) {
      super();
      cnt = 0;
      baseCallCnt = 0;
      this.catchInCalled = catchInCalled;
    }

    public void run() {
      baseCallCnt++;
      try {
        result = calledFunction();
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " thrown and caught!");
      }
      baseCallCnt++;
    }

    public Object calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          cnt += 10;
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println(e.getClass().getName() + " caught in same function.");
          Object result = new IntContainer(cnt);
          cnt += 100;
          return result;
        }
      } else {
        cnt += 1;
        throw new TestError(); // We put a watch here.
      }
    }

    public String toString() {
      return "ExceptionThrowTestObject { cnt: " + cnt + ", baseCnt: " + baseCallCnt + " }";
    }
  }

  public static class NativeCalledObject extends AbstractTestObject {
    public int cnt = 0;

    public native Object calledFunction();

    public String toString() {
      return "NativeCalledObject { cnt: " + cnt + " }";
    }
  }

  public static class NativeCallerObject implements TestRunnable {
    public Object returnValue = null;
    public int cnt = 0;

    public Object getReturnValue() {
      return returnValue;
    }

    public native void run();

    public Object calledFunction() {
      cnt++;
      // We will stop using a MethodExit event.
      Object res = new IntContainer(cnt);
      cnt++;
      return res;
    }

    public String toString() {
      return "NativeCallerObject { cnt: " + cnt + " }";
    }
  }

  // public static class SuspendSuddenlyObject extends AbstractTestObject {
  // public volatile boolean stop_spinning = false;
  // public volatile boolean is_spinning = false;
  // public int cnt = 0;

  // public void calledFunction() {
  // cnt++;
  // while (!stop_spinning) {
  // is_spinning = true;
  // }
  // }

  // public String toString() {
  // return "SuspendSuddenlyObject { cnt: " + cnt + " }";
  // }
  // }

  public static void run(boolean canRunClassLoadTests) throws Exception {
    new Test1960(canRunClassLoadTests, (x) -> {}).runTests();
  }

  // This entrypoint is used by CTS only. */
  public static void run() throws Exception {
    /*
     * TODO: Due to the way that CTS tests are verified we cannot run
     * class-load-tests since the verifier will be delayed until runtime and then
     * load the classes all at once. This makes the test impossible to run.
     */
    run(/* canRunClassLoadTests */ false);
  }

  public Test1960(boolean canRunClassLoadTests, Consumer<TestRunnable> preTest) {
    this.canRunClassLoadTests = canRunClassLoadTests;
    this.preTest = preTest;
  }

  private Consumer<TestRunnable> preTest;

  public static void no_runTestOn(Supplier<Object> a, ThreadRunnable b, ThreadRunnable c) {}

  public void runTests() throws Exception {
    setupTest();

    final Method calledFunction = StandardTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    final int line = Breakpoint.locationToLine(calledFunction, 0) + 2;
    final long loc = Breakpoint.lineToLocation(calledFunction, line);
    System.out.println("Test stopped using breakpoint");
    runTestOn(
        StandardTestObject::new,
        (thr) -> setupSuspendBreakpointFor(calledFunction, loc, thr),
        Test1960::clearSuspendBreakpointFor);

    final Method syncFunctionCalledFunction =
        SynchronizedFunctionTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    // Annoyingly r8 generally
    // has the first instruction (a monitor enter) not be marked as being on any
    // line but javac has
    // it marked as being on the first line of the function. Just use the second
    // entry on the
    // line-number table to get the breakpoint. This should be good for both.
    final long syncFunctionLoc =
        Breakpoint.getLineNumberTable(syncFunctionCalledFunction)[1].location;
    System.out.println("Test stopped using breakpoint with declared synchronized function");
    runTestOn(
        SynchronizedFunctionTestObject::new,
        (thr) -> setupSuspendBreakpointFor(syncFunctionCalledFunction, syncFunctionLoc, thr),
        Test1960::clearSuspendBreakpointFor);

    final Method syncCalledFunction =
        SynchronizedTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    final int syncLine = Breakpoint.locationToLine(syncCalledFunction, 0) + 3;
    final long syncLoc = Breakpoint.lineToLocation(syncCalledFunction, syncLine);
    System.out.println("Test stopped using breakpoint with synchronized block");
    runTestOn(
        SynchronizedTestObject::new,
        (thr) -> setupSuspendBreakpointFor(syncCalledFunction, syncLoc, thr),
        Test1960::clearSuspendBreakpointFor);

    System.out.println("Test stopped on single step");
    runTestOn(
        StandardTestObject::new,
        (thr) -> setupSuspendSingleStepAt(calledFunction, loc, thr),
        Test1960::clearSuspendSingleStepFor);

    final Field target_field = FieldBasedTestObject.class.getDeclaredField("TARGET_FIELD");
    System.out.println("Test stopped on field access");
    runTestOn(
        FieldBasedTestObject::new,
        (thr) -> setupFieldSuspendFor(FieldBasedTestObject.class, target_field, true, thr),
        Test1960::clearFieldSuspendFor);

    System.out.println("Test stopped on field modification");
    runTestOn(
        FieldBasedTestObject::new,
        (thr) -> setupFieldSuspendFor(FieldBasedTestObject.class, target_field, false, thr),
        Test1960::clearFieldSuspendFor);

    System.out.println("Test stopped during Method Exit of calledFunction");
    runTestOn(
        StandardTestObject::new,
        (thr) -> setupSuspendMethodEvent(calledFunction, /* enter */ false, thr),
        Test1960::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Enter of calledFunction");
    runTestOn(
        StandardTestObject::new,
        (thr) -> setupSuspendMethodEvent(calledFunction, /* enter */ true, thr),
        Test1960::clearSuspendMethodEvent);

    final Method exceptionOnceCalledMethod =
        ExceptionOnceObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during Method Exit due to exception thrown in same function");
    runTestOn(
        () -> new ExceptionOnceObject(/* throwInSub */ false),
        (thr) -> setupSuspendMethodEvent(exceptionOnceCalledMethod, /* enter */ false, thr),
        Test1960::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Exit due to exception thrown in subroutine");
    runTestOn(
        () -> new ExceptionOnceObject(/* throwInSub */ true),
        (thr) -> setupSuspendMethodEvent(exceptionOnceCalledMethod, /* enter */ false, thr),
        Test1960::clearSuspendMethodEvent);

    final Method exceptionThrowCalledMethod =
        ExceptionThrowTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println(
        "Test stopped during notifyFramePop with exception on pop of calledFunction");
    runTestOn(
        () -> new ExceptionThrowTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(0, exceptionThrowCalledMethod, thr),
        Test1960::clearSuspendPopFrameEvent);

    final Method exceptionCatchThrowMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("doThrow");
    System.out.println("Test stopped during notifyFramePop with exception on pop of doThrow");
    runTestOn(
        ExceptionCatchTestObject::new,
        (thr) -> setupSuspendPopFrameEvent(0, exceptionCatchThrowMethod, thr),
        Test1960::clearSuspendPopFrameEvent);

    System.out.println(
        "Test stopped during ExceptionCatch event of calledFunction "
            + "(catch in called function, throw in called function)");
    runTestOn(
        () -> new ExceptionThrowTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /* catch */ true, thr),
        Test1960::clearSuspendExceptionEvent);

    final Method exceptionCatchCalledMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println(
        "Test stopped during ExceptionCatch event of calledFunction "
            + "(catch in called function, throw in subroutine)");
    runTestOn(
        ExceptionCatchTestObject::new,
        (thr) -> setupSuspendExceptionEvent(exceptionCatchCalledMethod, /* catch */ true, thr),
        Test1960::clearSuspendExceptionEvent);

    System.out.println(
        "Test stopped during Exception event of calledFunction " + "(catch in calling function)");
    runTestOn(
        () -> new ExceptionThrowTestObject(false),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /* catch */ false, thr),
        Test1960::clearSuspendExceptionEvent);

    System.out.println(
        "Test stopped during Exception event of calledFunction (catch in called function)");
    runTestOn(
        () -> new ExceptionThrowTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /* catch */ false, thr),
        Test1960::clearSuspendExceptionEvent);

    final Method exceptionThrowFarCalledMethod =
        ExceptionThrowFarTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println(
        "Test stopped during Exception event of calledFunction "
            + "(catch in parent of calling function)");
    runTestOn(
        () -> new ExceptionThrowFarTestObject(false),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowFarCalledMethod, /* catch */ false, thr),
        Test1960::clearSuspendExceptionEvent);

    System.out.println(
        "Test stopped during Exception event of calledFunction " + "(catch in called function)");
    runTestOn(
        () -> new ExceptionThrowFarTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowFarCalledMethod, /* catch */ false, thr),
        Test1960::clearSuspendExceptionEvent);

    // System.out.println("Test stopped during random Suspend.");
    // final SuspendSuddenlyObject sso = new SuspendSuddenlyObject();
    // runTestOn(sso, new TestSuspender() {
    // public void setup(Thread thr) {
    // }

    // public void waitForSuspend(Thread thr) {
    // while (!sso.is_spinning) {
    // }
    // Suspension.suspend(thr);
    // }

    // public void cleanup(Thread thr) {
    // sso.stop_spinning = true;
    // }
    // });

    System.out.println("Test stopped during a native method fails");
    runTestOn(NativeCalledObject::new, Test1960::setupWaitForNativeCall, Test1960::clearWaitForNativeCall);

    System.out.println("Test stopped in a method called by native succeeds");
    final Method nativeCallerMethod =
    NativeCallerObject.class.getDeclaredMethod("calledFunction");
    runTestOn(NativeCallerObject::new, (thr) ->
    setupSuspendMethodEvent(nativeCallerMethod, /* enter */ false, thr),
    Test1960::clearSuspendMethodEvent);
  }

  // Volatile is to prevent any future optimizations that could invalidate this
  // test by doing
  // constant propagation and eliminating the failing paths before the verifier is
  // able to load
  // the class.
  static volatile boolean ranClassLoadTest = false;
  static boolean classesPreverified = false;

  private static final class RCLT0 {
    public void foo() {}
  }

  private static final class RCLT1 {
    public void foo() {}
  }

  // If classes are not preverified for some reason (interp-ac, no-image, etc) the
  // verifier will
  // actually load classes as it runs. This means that we cannot use the
  // class-load tests as they
  // are written. TODO Support this.
  public boolean CanRunClassLoadingTests() {
    if (ranClassLoadTest) {
      return classesPreverified;
    }
    if (!ranClassLoadTest) {
      // Only this will ever be executed.
      new RCLT0().foo();
    } else {
      // This will never be executed. If classes are not preverified the verifier will
      // load RCLT1
      // when the enclosing method is run. This behavior makes the class-load/prepare
      // test cases
      // impossible to successfully run (they will deadlock).
      new RCLT1().foo();
      System.out.println("FAILURE: UNREACHABLE Location!");
    }
    classesPreverified = !isClassLoaded("Lart/Test1960$RCLT1;");
    ranClassLoadTest = true;
    return classesPreverified;
  }

  public static native boolean isClassLoaded(String name);

  public static native void setupTest();

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
      int eventType, String[] interestingNames, Thread thr);

  public static native void clearSuspendClassEvent(Thread thr);

  public static native void setupWaitForNativeCall(Thread thr);

  public static native void clearWaitForNativeCall(Thread thr);

  public static native void waitForSuspendHit(Thread thr);
}
