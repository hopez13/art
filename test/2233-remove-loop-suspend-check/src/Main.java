/*
 * Copyright (C) 2022 The Android Open Source Project
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

abstract class Base {
  abstract int foo();
}

class Main1 extends Base {
  int foo() {
    return 1;
  }
}

public class Main {
  static Base sMain1;

  // sMain1.foo() will be always be Main1.foo() so sMain1.foo() can be
  // devirtualized to Main1.foo() which creates a CHA guard. Next the call will
  // be inlined which allows for the SuspendCheck to be removed however we
  // should not do so as there are optimizations on CHA guards which will fail
  // without a SuspendCheck present.
  static void testRemoveSuspendCheckWithChaGuard(int[] arr) {
    Base main1_test = sMain1;

    for (int i = 0; i < 5; i++) {
      arr[i] = main1_test.foo();
    }
  }

  // Test scenarios under which SuspendChecks should or should not be removed
  // from certain loops.
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    // sMain1 is an instance of Main1.
    sMain1 = new Main1();
    int[] arr = new int[100];

    ensureJitCompiled(Main.class, "testRemoveSuspendCheckWithChaGuard");
    testRemoveSuspendCheckWithChaGuard(arr);
  }

  private static native void ensureJitCompiled(Class<?> itf, String method_name);
}
