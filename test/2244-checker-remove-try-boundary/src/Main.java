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
    assertEquals(2, $noinline$testDivideOverTen(20));
    assertEquals(-2, $noinline$testDivideOverTen(-20));
    assertEquals(0, $noinline$testSimpleDivisionInLoop(0));
    assertEquals(1, $noinline$testSimpleDivisionInLoop(81));
    assertEquals(1, $noinline$testDoNotOptimizeSeparateBranches(6, true));
    assertEquals(1, $noinline$testDoNotOptimizeSeparateBranches(8, true));
  }

  public static void assertEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // Check that this version cannot remove the TryBoundary instructions

  /// CHECK-START: int Main.$inline$division(int, int) register (after)
  /// CHECK:     TryBoundary
  /// CHECK:     TryBoundary
  /// CHECK-NOT: TryBoundary
  private static int $inline$division(int a, int b) {
    try {
      return a / b;
    } catch (Error unexpected) {
      return -1000;
    }
  }

  // Check that we can remove the TryBoundary afer inlining since we know we can't throw.

  /// CHECK-START: int Main.$noinline$testDivideOverTen(int) inliner (after)
  /// CHECK-NOT: TryBoundary

  /// CHECK-START: int Main.$noinline$testDivideOverTen(int) inliner (after)
  /// CHECK-NOT: flags "catch_block"
  private static int $noinline$testDivideOverTen(int a) {
    return $inline$division(a, 10);
  }

  /// CHECK-START: int Main.$noinline$testSimpleDivisionInLoop(int) dead_code_elimination$initial (before)
  /// CHECK:     TryBoundary
  /// CHECK:     TryBoundary
  /// CHECK-NOT: TryBoundary

  /// CHECK-START: int Main.$noinline$testSimpleDivisionInLoop(int) dead_code_elimination$initial (before)
  /// CHECK:     flags "catch_block"

  /// CHECK-START: int Main.$noinline$testSimpleDivisionInLoop(int) dead_code_elimination$initial (after)
  /// CHECK-NOT: TryBoundary

  /// CHECK-START: int Main.$noinline$testSimpleDivisionInLoop(int) dead_code_elimination$initial (after)
  /// CHECK-NOT: flags "catch_block"
  private static int $noinline$testSimpleDivisionInLoop(int a) {
    try {
      for (int i = 0; i < 4; i++) {
        a /= 3;
      }
    } catch (Error unexpected) {
      return -1000;
    }
    return a;
  }

  // Even though we could potentially remove these TryBoundaries, we have some split try boundaries
  // which should be dealt with carefully.

  /// CHECK-START: int Main.$noinline$testDoNotOptimizeSeparateBranches(int, boolean) register (after)
  /// CHECK:     TryBoundary
  /// CHECK:     TryBoundary
  /// CHECK:     TryBoundary
  /// CHECK-NOT: TryBoundary
  private static int $noinline$testDoNotOptimizeSeparateBranches(int a, boolean val) {
    try {
      if (val) {
        // TryBoundary kind:entry
        a /= 3;
      } else {
        // TryBoundary kind:entry
        a /= 4;
      }
      a /= 2;
      // TryBoundary kind:exit
    } catch (Error unexpected) {
      return -1000;
    }
    return a;
  }
}
