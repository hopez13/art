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

public class Main {
  public static void main(String[] args) throws Exception {
    assertEquals(0, testSimplifyThrow(1));
    assertEquals(0, testSimplifyThrowWithTryCatch(1));
  }

  private static void alwaysThrows() throws Error {
    throw new Error("");
  }

  /// CHECK-START: int Main.testSimplifyThrow(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       Return
  /// CHECK:       Exit

  /// CHECK-START: int Main.testSimplifyThrow(int) dead_code_elimination$after_gvn (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK-NOT:   Return
  /// CHECK:       Exit

  // Tests that we simplify the always throwing branch directly to the exit.
  private static int testSimplifyThrow(int num) {
    if (num == 0) {
      alwaysThrows();
    }
    return 0;
  }

  /// CHECK-START: int Main.testSimplifyThrowWithTryCatch(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       If
  /// CHECK:       Exit

  /// CHECK-START: int Main.testSimplifyThrowWithTryCatch(int) dead_code_elimination$after_gvn (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK-NOT:   If
  /// CHECK:       Exit

  // Consistency check to make sure we have the try catches in the graph at this stage.
  /// CHECK-START: int Main.testSimplifyThrowWithTryCatch(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       TryBoundary kind:entry
  /// CHECK:       TryBoundary kind:entry

  // Tests that we simplify the always throwing branch directly to the exit, with non-blocking try
  // catches in the graph.
  private static int testSimplifyThrowWithTryCatch(int num) {
    try {
      if (num == 123) {
        throw new Error();
      }
    } catch (Error e) {
        return 123;
    }

    if (num == 0) {
      alwaysThrows();
    }

    try {
      if (num == 456) {
        throw new Error();
      }
    } catch (Error e) {
        return 456;
    }
    return 0;
  }

  static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }
}