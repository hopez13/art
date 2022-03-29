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
    // Basic test
    assertEquals(0, testSimplifyThrow(1));

    // Basic test for non-trivial blocks (i.e. not just an invoke and a Goto)
    assertEquals(0, testSimplifyTwoThrows(1));

    // Try catch tests
    assertEquals(0, testDoNotSimplifyInTry(1));
    assertEquals(0, testSimplifyInCatch(1));
    assertEquals(0, testDoNotSimplifyInCatchInOuterTry(1));
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

  /// CHECK-START: int Main.testSimplifyTwoThrows(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       Return
  /// CHECK:       Exit

  /// CHECK-START: int Main.testSimplifyTwoThrows(int) dead_code_elimination$after_gvn (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK-NOT:   Return
  /// CHECK:       Exit

  // Tests that we simplify the always throwing branch directly to the exit, even with blocks that
  // are not just the throwing instruction and a Goto.
  private static int testSimplifyTwoThrows(int num) {
    if (num == 0) {
      alwaysThrows();
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

  private static void $inline$testDoNotSimplifyInner(int num) {
    alwaysThrows();
    while (num == 0) {
      // We should never hit this since we are always throwing.
      System.out.println(num);
    }
  }

  /// CHECK-START: int Main.testDoNotSimplifyInTry(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK-NOT:   TryBoundary
  /// CHECK:       Goto
  /// CHECK:       If

  /// CHECK-START: int Main.testDoNotSimplifyInTry(int) dead_code_elimination$after_gvn (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK-NOT:   TryBoundary
  /// CHECK:       Goto
  /// CHECK:       If

  // Consistency check to make sure we have the try catch in the graph at this stage.
  /// CHECK-START: int Main.testDoNotSimplifyInTry(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       TryBoundary kind:entry

  // Consistency check to that we do not simplify it by the last DCE pass either
  /// CHECK-START: int Main.testDoNotSimplifyInTry(int) dead_code_elimination$final (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK-NOT:   TryBoundary
  /// CHECK:       Goto
  /// CHECK:       If

  // Tests that we have the necessary conditions for us to simplify the always throwing instruction
  // (e.g. InvokeStaticOrDirect followed by a Goto) but we are blocking this due to being in a try.
  // Changing the Goto here for the exit would be wrong since we want to flow to the catch rather
  // than the Exit. The preconditions are tricky to do with just one function (since we will have an
  // invoke followed by a TryBoundary rather than a Goto) but we can do so with the help of the
  // inliner.
  private static int testDoNotSimplifyInTry(int num) {
    try {
      $inline$testDoNotSimplifyInner(num);
    } catch (Error e) {
      return 0;
    }
    return 123;
  }

  /// CHECK-START: int Main.testSimplifyInCatch(int) dead_code_elimination$final (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       Return

  /// CHECK-START: int Main.testSimplifyInCatch(int) dead_code_elimination$after_gvn (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK-NOT:   Return
  /// CHECK:       Exit

  // Consistency check to make sure we have the try catch in the graph at this stage.
  /// CHECK-START: int Main.testSimplifyInCatch(int) dead_code_elimination$after_gvn (before)
  /// CHECK:       TryBoundary kind:entry

  // We are able to simplify the `alwaysThrows` even though we are inside of the catch { ... } since
  // the if makes it so that we are not the first block of the catch and therefore not in the
  // "catch_block".
  private static int testSimplifyInCatch(int num) {
    try {
      throw new Error();
    } catch (Error e) {
      if (num == 0) {
        alwaysThrows();
      }
      return 0;
    }
  }

  /// CHECK-START: int Main.testDoNotSimplifyInCatchInOuterTry(int) dead_code_elimination$final (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       If

  /// CHECK-START: int Main.testDoNotSimplifyInCatchInOuterTry(int) dead_code_elimination$after_gvn (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       If

  // Consistency check to make sure we have the try catches in the graph at this stage.
  /// CHECK-START: int Main.testDoNotSimplifyInCatchInOuterTry(int) dead_code_elimination$after_gvn (before)
  /// CHECK-DAG:   TryBoundary kind:entry
  /// CHECK-DAG:   TryBoundary kind:entry

  // Consistency check to that we do not simplify it by the last DCE pass either
  /// CHECK-START: int Main.testDoNotSimplifyInCatchInOuterTry(int) dead_code_elimination$final (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.alwaysThrows always_throws:true
  /// CHECK:       Goto
  /// CHECK:       If

  // Similar to testSimplifyInCatch, but now the throw is in an outer try and we shouldn't simplify
  // it. Like in testDoNotSimplifyInTry, we need the help of the inliner to have an invoke followed
  // by a Goto.
  private static int testDoNotSimplifyInCatchInOuterTry(int num) {
    try {
      try {
        throw new Error();
      } catch (Error e) {
        if (num == 0) {
          $inline$testDoNotSimplifyInner(num);
        }
        return 0;
      }
    } catch (Error e) {
      return 123;
    }
  }

  static void assertEquals(int expected, int actual) {
    if (expected != actual) {
      throw new AssertionError("Expected " + expected + " got " + actual);
    }
  }
}