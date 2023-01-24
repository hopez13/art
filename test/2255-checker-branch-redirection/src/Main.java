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
    assertEquals(40, $noinline$testEliminateBranch(20, 40));
    assertEquals(30, $noinline$testEliminateBranch(20, 10));
    assertEquals(25, $noinline$testEliminateBranchTwiceInARow(20, 40));
    assertEquals(43, $noinline$testEliminateBranchTwiceInARow(20, 10));
    assertEquals(40, $noinline$testEliminateBranchThreePredecessors(20, 40));
    assertEquals(30, $noinline$testEliminateBranchThreePredecessors(20, 10));
    assertEquals(40, $noinline$testEliminateBranchOppositeCondition(20, 40));
    assertEquals(30, $noinline$testEliminateBranchOppositeCondition(20, 10));
  }

  private static int $noinline$emptyMethod(int a) {
    return a;
  }

  /// CHECK-START: int Main.$noinline$testEliminateBranch(int, int) dead_code_elimination$after_gvn (before)
  /// CHECK:     If
  /// CHECK:     If

  /// CHECK-START: int Main.$noinline$testEliminateBranch(int, int) dead_code_elimination$after_gvn (after)
  /// CHECK:     If
  /// CHECK-NOT: If
  private static int $noinline$testEliminateBranch(int a, int b) {
    int result = 0;
    if (a < b) {
      $noinline$emptyMethod(a + b);
    } else {
      $noinline$emptyMethod(a - b);
    }
    if (a < b) {
      result += $noinline$emptyMethod(a * 2);
    } else {
      result += $noinline$emptyMethod(b * 3);
    }
    return result;
  }

  /// CHECK-START: int Main.$noinline$testEliminateBranchTwiceInARow(int, int) dead_code_elimination$after_gvn (before)
  /// CHECK:     If
  /// CHECK:     If
  /// CHECK:     If

  /// CHECK-START: int Main.$noinline$testEliminateBranchTwiceInARow(int, int) dead_code_elimination$after_gvn (after)
  /// CHECK:     If
  /// CHECK-NOT: If
  private static int $noinline$testEliminateBranchTwiceInARow(int a, int b) {
    int result = 0;
    if (a < b) {
      $noinline$emptyMethod(a + b);
    } else {
      $noinline$emptyMethod(a - b);
    }
    if (a < b) {
      $noinline$emptyMethod(a * 2);
    } else {
      $noinline$emptyMethod(b * 3);
    }
    if (a < b) {
      result += $noinline$emptyMethod(25);
    } else {
      result += $noinline$emptyMethod(43);
    }
    return result;
  }

  /// CHECK-START: int Main.$noinline$testEliminateBranchThreePredecessors(int, int) dead_code_elimination$after_gvn (before)
  /// CHECK:     If
  /// CHECK:     If
  /// CHECK:     If

  /// CHECK-START: int Main.$noinline$testEliminateBranchThreePredecessors(int, int) dead_code_elimination$after_gvn (after)
  /// CHECK:     If
  /// CHECK:     If
  /// CHECK-NOT: If
  private static int $noinline$testEliminateBranchThreePredecessors(int a, int b) {
    int result = 0;
    if (a < b) {
        $noinline$emptyMethod(a + b);
    } else {
      if (b < 5) {
        $noinline$emptyMethod(a - b);
      } else {
        $noinline$emptyMethod(a * b);
      }
    }
    if (a < b) {
      result += $noinline$emptyMethod(a * 2);
    } else {
      result += $noinline$emptyMethod(b * 3);
    }
    return result;
  }

  /// CHECK-START: int Main.$noinline$testEliminateBranchOppositeCondition(int, int) dead_code_elimination$after_gvn (before)
  /// CHECK:     If
  /// CHECK:     If

  /// CHECK-START: int Main.$noinline$testEliminateBranchOppositeCondition(int, int) dead_code_elimination$after_gvn (after)
  /// CHECK:     If
  /// CHECK-NOT: If
  private static int $noinline$testEliminateBranchOppositeCondition(int a, int b) {
    int result = 0;
    if (a < b) {
      $noinline$emptyMethod(a + b);
    } else {
      $noinline$emptyMethod(a - b);
    }
    if (a >= b) {
      result += $noinline$emptyMethod(b * 3);
    } else {
      result += $noinline$emptyMethod(a * 2);
    }
    return result;
  }

  public static void assertEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
