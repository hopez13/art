/*
 * Copyright (C) 2021 The Android Open Source Project
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

interface Itf {
  // We make the methods below directly throw instead of using the $noinline$
  // directive to get the inliner actually try to inline but decide not to.
  // This will then make the compiler try to generate an HInvokeVirtual instead
  // of an HInvokeInterface.

  public default void m() throws Exception {
    throw new Exception("Don't inline me");
  }
  public default void mConflict() throws Exception {
    throw new Exception("Don't inline me");
  }
}

// This is redefined in src2 with a mConflict method.
interface Itf2 {
}

public class Main implements Itf, Itf2 {

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    ensureJitCompiled(Main.class, "$noinline$doCallDefault");
    try {
      $noinline$doCallDefault();
      throw new Error("Expected exception");
    } catch (Exception e) {
      // Expected.
    }

    ensureJitCompiled(Main.class, "$noinline$doCallDefaultConflict");
    try {
      $noinline$doCallDefaultConflict();
      throw new Error("Expected exception");
    } catch (Exception e) {
      // Expected.
    }
  }

  public static void $noinline$doCallDefault() throws Exception {
    itf.m();
  }

  public static void $noinline$doCallDefaultConflict() throws Exception {
    itf.mConflict();
  }

  static Itf itf = new Main();

  private static native void ensureJitCompiled(Class<?> cls, String methodName);
}
