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


import art.Redefinition;

import java.util.function.Consumer;
import java.lang.reflect.Method;
import java.util.Base64;

import static test_916_transforms.Transform.CLASS_BYTES;
import static test_916_transforms.Transform.DEX_BYTES;

public class Main {
  // A class that we can use to keep track of the output of this test.
  private static class TestWatcher implements Consumer<String> {
    private StringBuilder sb;
    public TestWatcher() {
      sb = new StringBuilder();
    }

    @Override
    public void accept(String s) {
      sb.append(s);
      sb.append('\n');
    }

    public String getOutput() {
      return sb.toString();
    }

    public void clear() {
      sb = new StringBuilder();
    }
  }

  public static void main(String[] args) {
    doTest(new Transform(), new TestWatcher());
  }

  private static boolean interpreting = true;
  private static boolean retry = false;

  public static void doTest(Transform t, TestWatcher w) {
    // Get the methods that need to be optimized.
    Method say_hi_method;
    // Figure out if we can even JIT at all.
    final boolean has_jit = hasJit();
    try {
      say_hi_method = Transform.class.getDeclaredMethod(
          "sayHi", Runnable.class, Consumer.class);
    } catch (Exception e) {
      System.out.println("Unable to find methods!");
      e.printStackTrace(System.out);
      return;
    }
    // Makes sure the stack is the way we want it for the test and does the redefinition. It will
    // set the retry boolean to true if the stack does not have a JIT-compiled sayHi entry. This can
    // only happen if the method gets GC'd.
    Runnable do_redefinition = () -> {
      if (has_jit && Main.isInterpretedFunction(say_hi_method, true)) {
        // Try again. We are not running the right jitted methods/cannot redefine them now.
        retry = true;
      } else {
        // Actually do the redefinition. The stack looks good.
        retry = false;
        w.accept("transforming calling function");
        Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
      }
    };
    // This just prints something out to show we are running the Runnable.
    Runnable say_nothing = () -> { w.accept("Not doing anything here"); };
    do {
      // Run ensureJitCompiled here since it might get GCd
      ensureJitCompiled(Transform.class, "sayHi");
      // Clear output.
      w.clear();
      // Try and redefine.
      t.sayHi(say_nothing, w);
      t.sayHi(do_redefinition, w);
      t.sayHi(say_nothing, w);
    } while (retry);
    // Print output of last run.
    System.out.print(w.getOutput());
  }

  private static native boolean hasJit();

  private static native boolean isInterpretedFunction(Method m, boolean require_deoptimizable);

  private static native void ensureJitCompiled(Class c, String name);
}
