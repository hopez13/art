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

import java.lang.reflect.Executable;
import java.lang.reflect.Constructor;
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

  public static class TestObject {
    public int cnt;
    public CountDownLatch latch;
    public TestObject(CountDownLatch latch) {
      super();
      this.latch = latch;
      cnt = 0;
    }

    public void callingFunction() {
      // Let other thread do setup.
      try {
        latch.await();
      } catch (Exception e) {
        throw new Error("Failed to await!", e);
      }
      // This function should be re-executed by the popFrame.
      calledFunction();
    }

    public void calledFunction() {
      cnt++;       // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      doNothing(); // line +2
    }

    public String toString() {
      return "TestObject { cnt: " + cnt + " }";
    }
  }

  public static void run() throws Exception {
    setupTest();
    CountDownLatch latch = new CountDownLatch(1);
    final TestObject obj = new TestObject(latch);
    System.out.println("Single call to callingFunction with PopFrame on " + obj + "!");
    Thread thr = new Thread(obj::callingFunction);
    thr.start();
    Method calledFunction = TestObject.class.getDeclaredMethod("calledFunction");

    // Add a breakpoint on the second line after the start of the function
    int line = Breakpoint.locationToLine(calledFunction, 0) + 2;
    setupSuspendBreakpointFor(calledFunction, Breakpoint.lineToLocation(calledFunction, line), thr);

    // Let the other thread go.
    latch.countDown();

    // Wait for the other thread to hit the breakpoint and suspend itself (without re-entering java)
    waitForSuspendHit(thr);
    // Clean up. we only want to hit this once.
    clearSuspendBreakpointFor(thr);
    // Pop the frame.
    popFrame(thr);
    // Start the other thread going again.
    Suspension.resume(thr);
    // Wait for the other thread to finish.
    thr.join();
    // See how many times calledFunction was called.
    System.out.println("result is " + obj);
  }
  public static native void setupTest();
  public static native void popFrame(Thread thr);

  public static native void setupSuspendBreakpointFor(Executable meth, long loc, Thread thr);
  public static native void clearSuspendBreakpointFor(Thread thr);

  public static native void waitForSuspendHit(Thread thr);
}
