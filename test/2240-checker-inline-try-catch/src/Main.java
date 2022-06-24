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
  public static void main(String[] args) {
    $noinline$testSingleTryCatch();
    $noinline$testSingleTryCatchTwice();
    $noinline$testDifferentTryCatches();
    $noinline$testTryCatchFinally();
    $noinline$testRecursiveTryCatch();
    $noinline$testDoNotInlineInsideTryOrCatch();
    $noinline$testBeforeAfterTryCatch();
    $noinline$testDifferentTypes();
    $noinline$testRawThrow();
    $noinline$testRawThrowTwice();
    $noinline$testThrowCaughtInOuterMethod();
  }

  public static void $noinline$assertEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // Basic try catch inline.
  private static void $noinline$testSingleTryCatch() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));
  }

  // Two instances of the same method with a try catch.
  private static void $noinline$testSingleTryCatchTwice() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));
    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));
  }

  // Two different try catches, with the same catch's dex_pc.
  private static void $noinline$testDifferentTryCatches() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));
    $noinline$assertEquals(2, $inline$OtherOOBTryCatch(numbers));
  }

  // Basic try/catch/finally.
  private static void $noinline$testTryCatchFinally() {
    int[] numbers = {};
    $noinline$assertEquals(3, $inline$OOBTryCatchFinally(numbers));
  }

  // Test that we can inline even when the try catch is several levels deep.
  private static void $noinline$testRecursiveTryCatch() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$RecursiveOOBTryCatchLevel3(numbers));
  }

  // Tests that we don't inline inside outer tries or catches.
  /// CHECK-START: void Main.$noinline$testDoNotInlineInsideTryOrCatch() inliner (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.DoNotInlineOOBTryCatch
  /// CHECK:       InvokeStaticOrDirect method_name:Main.DoNotInlineOOBTryCatch

  /// CHECK-START: void Main.$noinline$testDoNotInlineInsideTryOrCatch() inliner (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Main.DoNotInlineOOBTryCatch
  /// CHECK:       InvokeStaticOrDirect method_name:Main.DoNotInlineOOBTryCatch
  private static void $noinline$testDoNotInlineInsideTryOrCatch() {
    int val = 0;
    try {
      int[] numbers = {};
      val = DoNotInlineOOBTryCatch(numbers);
    } catch (Exception ex) {
      unreachable();
      // This is unreachable but we will still compile it so it works for checker purposes
      int[] numbers = {};
      DoNotInlineOOBTryCatch(numbers);
    }
    $noinline$assertEquals(1, val);
  }

  // Tests that outer tries or catches don't affect as long as we are not inlining the inner
  // try/catch inside of them.
  private static void $noinline$testBeforeAfterTryCatch() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));

    // Unrelated try catch does not block inlining outside of it. We fill it in to make sure it is
    // still there by the time the inliner runs.
    int val = 0;
    try {
      int[] other_array = {};
      val = other_array[0];
    } catch (Exception ex) {
      $noinline$assertEquals(0, val);
      val = 1;
    }
    $noinline$assertEquals(1, val);

    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));
  }

  // Tests different try catch types in the same outer method.
  private static void $noinline$testDifferentTypes() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$OOBTryCatch(numbers));
    $noinline$assertEquals(2, $inline$OtherOOBTryCatch(numbers));
    $noinline$assertEquals(123, $inline$ParseIntTryCatch("123"));
    $noinline$assertEquals(-1, $inline$ParseIntTryCatch("abc"));
  }

  // Tests a raw throw (rather than an instruction that happens to throw).
  private static void $noinline$testRawThrow() {
    $noinline$assertEquals(1, $inline$rawThrowCaught());
  }

  // Tests a raw throw twice.
  private static void $noinline$testRawThrowTwice() {
    $noinline$assertEquals(1, $inline$rawThrowCaught());
    $noinline$assertEquals(1, $inline$rawThrowCaught());
  }

  // Tests that the outer method can successfully catch the throw in the inner method.
  private static void $noinline$testThrowCaughtInOuterMethod() {
    int[] numbers = {};
    $noinline$assertEquals(1, $inline$testThrowCaughtInOuterMethod_simpleTryCatch(numbers));
    $noinline$assertEquals(1, $inline$testThrowCaughtInOuterMethod_withFinally(numbers));
  }

  // Building blocks for the test functions.
  private static int $inline$OOBTryCatch(int[] array) {
    try {
      return array[0];
    } catch (Exception e) {
      return 1;
    }
  }

  private static int $inline$OtherOOBTryCatch(int[] array) {
    try {
      return array[0];
    } catch (Exception e) {
      return 2;
    }
  }

  private static int $inline$OOBTryCatchFinally(int[] array) {
    int val = 0;
    try {
      val = 1;
      return array[0];
    } catch (Exception e) {
      val = 2;
    } finally {
      $noinline$assertEquals(2, val);
      val = 3;
    }
    return val;
  }

  // If we make the recursiveness a parameter, we wouldn't be able to mark as $inline$ and we would
  // need extra CHECKer statements.
  private static int $inline$RecursiveOOBTryCatchLevel3(int[] array) {
    return $inline$RecursiveOOBTryCatchLevel2(array);
  }

  private static int $inline$RecursiveOOBTryCatchLevel2(int[] array) {
    return $inline$RecursiveOOBTryCatchLevel1(array);
  }

  private static int $inline$RecursiveOOBTryCatchLevel1(int[] array) {
    return $inline$RecursiveOOBTryCatchLevel0(array);
  }

  private static int $inline$RecursiveOOBTryCatchLevel0(int[] array) {
    return $inline$OOBTryCatch(array);
  }

  private static int DoNotInlineOOBTryCatch(int[] array) {
    try {
      return array[0];
    } catch (Exception e) {
      return 1;
    }
  }

  private static void unreachable() {
    throw new Error("Unreachable");
  }

  private static int $inline$ParseIntTryCatch(String str) {
    try {
      return Integer.parseInt(str);
    } catch (NumberFormatException ex) {
      return -1;
    }
  }

  private static int $inline$rawThrowCaught() {
    try {
      throw new Error();
    } catch (Error e) {
      return 1;
    }
  }

  private static int $inline$testThrowCaughtInOuterMethod_simpleTryCatch(int[] array) {
    int val = 0;
    try {
      throwingMethod(array);
    } catch (Exception ex) {
      val = 1;
    }
    return val;
  }

  private static int throwingMethod(int[] array) {
    return array[0];
  }

  private static int $inline$testThrowCaughtInOuterMethod_withFinally(int[] array) {
    int val = 0;
    try {
      throwingMethodWithFinally(array);
    } catch (Exception ex) {
      System.out.println("Our battle it will be legendary!");
      val = 1;
    }
    return val;
  }

  private static int throwingMethodWithFinally(int[] array) {
    try {
      return array[0];
    } finally {
      System.out.println("Finally, a worthy opponent!");
    }
  }
}
