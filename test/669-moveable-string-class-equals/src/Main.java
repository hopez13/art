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

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    if (!hasJit()) {
      System.out.println("NO JIT!");
      return;
    }
    if (hasImage()) {
      System.out.println("IMAGE!");
      return;
    }
    if (!isStringClassMoveable()) {
      System.out.println("String.class not moveable!");
      return;
    }

    ensureJitCompiled(Main.class, "test");
    test();
  }

  public static void test() {
    int length = 10;
    A_array1 = new String[length];
    z_array2 = new Object[length];
    for (int i = 0; i != length; ++i) {
      A_array1[i] = "V" + i;
      z_array2[i] = "V" + i;
    }

    for (int count = 0; count != 128 * 1024; ++count) {
      for (int i = length; i != 0; ) {
        --i;
        allocateAtLeast1KiB();
        assertTrue(A_array1[i].equals(z_array2[i]));
      }
    }
  }

  public static void allocateAtLeast1KiB() {
    // Give GC more work by allocating Object arrays.
    memory[allocationIndex] = new Object[1024 / 4];
    ++allocationIndex;
    if (allocationIndex == memory.length) {
      allocationIndex = 0;
    }
  }

  public static void assertTrue(boolean value) {
    if (!value) {
      throw new Error("Assertion failed!");
    }
  }

  private native static boolean hasJit();
  private native static boolean hasImage();
  private native static boolean isStringClassMoveable();
  private static native void ensureJitCompiled(Class<?> itf, String method_name);

  // We shall retain some allocated memory and release old allocations
  // so that the GC has something to do.
  public static Object[] memory = new Object[4096];
  public static int allocationIndex = 0;

  // Prefix arrays so that one comes before and the other comes after `memory`,
  // so that the GC takes some time between processing their elements.
  public static String[] A_array1;
  public static Object[] z_array2;
}
