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

import java.lang.reflect.Method;

public class Main {

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static <T> T $noinline$runSmaliTest(String name, Class<T> klass, T input1, T input2) {
    if (doThrow) { throw new Error(); }

    Class<T> inputKlass = (Class<T>)input1.getClass();
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name, klass, klass);
      return inputKlass.cast(m.invoke(null, input1, input2));
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  /**
   * Test transformation of Not/Not/And into Or/Not.
   */

  // Note: Both XOR or NOT can be used depending on the dexer.
  //       See:
  //            SmaliTests.$opt$noinline$andToOrUsingNot
  //            SmaliTests.$opt$noinline$andToOrUsingXor

  /// CHECK-START: int Main.$opt$noinline$andToOr(int, int) instruction_simplifier (before)
  /// CHECK-DAG:       <<P1:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<And:i\d+>>         And [{{i\d+}},{{i\d+}}]
  /// CHECK-DAG:                            Return [<<And>>]

  /// CHECK-START: int Main.$opt$noinline$andToOr(int, int) instruction_simplifier (after)
  /// CHECK-DAG:       <<P1:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<Or:i\d+>>          Or [<<P1>>,<<P2>>]
  /// CHECK-DAG:       <<Not:i\d+>>         Not [<<Or>>]
  /// CHECK-DAG:                            Return [<<Not>>]

  /// CHECK-START: int Main.$opt$noinline$andToOr(int, int) instruction_simplifier (after)
  /// CHECK-DAG:                            Not
  /// CHECK-NOT:                            Not

  /// CHECK-START: int Main.$opt$noinline$andToOr(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                            And

  public static int $opt$noinline$andToOr(int a, int b) {
    if (doThrow) throw new Error();
    return ~a & ~b;
  }

  /**
   * Depending on the dexer, the generated code can use either control flow:
   *  if (x) { y = false; } else { y = true; } 
   * or XOR to implememnt boolean "not".
   *
   * Detailed checks can be found in:
   *  SmaliTests.$opt$noinline$booleanAndToOrWithBranch
   *  SmaliTests.$opt$noinline$booleanAndToOrWithXor
   */
  /// CHECK-START: boolean Main.$opt$noinline$booleanAndToOr(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK-DAG:                            BooleanNot
  /// CHECK-NOT:                            BooleanNot

  /// CHECK-START: boolean Main.$opt$noinline$booleanAndToOr(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK-NOT:                            And
  public static boolean $opt$noinline$booleanAndToOr(boolean a, boolean b) {
    if (doThrow) throw new Error();
    return !a & !b;
  }

  /**
   * Test transformation of Not/Not/Or into And/Not.
   *
   * Depending on the dexer, the ~ can generated with either  Xor or Not.
   * See:
   *  SmaliTests.$opt$noinline$orToAndWithXor
   *  SmaliTests.$opt$noinline$orToAndWIthNot
   */
  /// CHECK-START: long Main.$opt$noinline$orToAnd(long, long) instruction_simplifier (before)
  /// CHECK-DAG:       <<P1:j\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:j\d+>>          ParameterValue
  /// CHECK-DAG:       <<Or:j\d+>>          Or [{{j\d+}},{{j\d+}}]
  /// CHECK-DAG:                            Return [<<Or>>]

  /// CHECK-START: long Main.$opt$noinline$orToAnd(long, long) instruction_simplifier (after)
  /// CHECK-DAG:       <<P1:j\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:j\d+>>          ParameterValue
  /// CHECK-DAG:       <<And:j\d+>>         And [<<P1>>,<<P2>>]
  /// CHECK-DAG:       <<Not:j\d+>>         Not [<<And>>]
  /// CHECK-DAG:                            Return [<<Not>>]

  /// CHECK-START: long Main.$opt$noinline$orToAnd(long, long) instruction_simplifier (after)
  /// CHECK-DAG:                            Not
  /// CHECK-NOT:                            Not

  /// CHECK-START: long Main.$opt$noinline$orToAnd(long, long) instruction_simplifier (after)
  /// CHECK-NOT:                            Or

  public static long $opt$noinline$orToAnd(long a, long b) {
    if (doThrow) throw new Error();
    return ~a | ~b;
  }

  /**
   * Test transformation of Not/Not/Or into Or/And for boolean negations.
   * Note that the graph before this instruction simplification pass does not
   * contain `HBooleanNot` instructions. This is because this transformation
   * follows the optimization of `HSelect` to `HBooleanNot` occurring in the
   * same pass.
   */

  // Note: Both branches or NOT can be used depending on the dexer.
  //       See:
  //            SmaliTests.$opt$noinline$booleanOrToAndWithBranch
  //            SmaliTests.$opt$noinline$booleanOrToAndWithXor

  /// CHECK-START: boolean Main.$opt$noinline$booleanOrToAnd(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK-DAG:                            BooleanNot
  /// CHECK-NOT:                            BooleanNot

  /// CHECK-START: boolean Main.$opt$noinline$booleanOrToAnd(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK-NOT:                            Or

  public static boolean $opt$noinline$booleanOrToAnd(boolean a, boolean b) {
    if (doThrow) throw new Error();
    return !a | !b;
  }

  /**
   * Test that the transformation copes with inputs being separated from the
   * bitwise operations.
   * This is a regression test. The initial logic was inserting the new bitwise
   * operation incorrectly.
   */

  // Note: Both XOR or NOT can be used depending on the dexer.
  //       See:
  //            SmaliTests.$opt$noinline$regressInputsAwayWithNot
  //            SmaliTests.$opt$noinline$regressInputsAwayWithXor

  /// CHECK-START: int Main.$opt$noinline$regressInputsAway(int, int) instruction_simplifier (after)
  /// CHECK-DAG:                            Not
  /// CHECK-NOT:                            Not

  /// CHECK-START: int Main.$opt$noinline$regressInputsAway(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                            Or

  public static int $opt$noinline$regressInputsAway(int a, int b) {
    if (doThrow) throw new Error();
    int a1 = a + 1;
    int not_a1 = ~a1;
    int b1 = b + 1;
    int not_b1 = ~b1;
    return not_a1 | not_b1;
  }

  /**
   * Test transformation of Not/Not/Xor into Xor.
   */

  // See first note above.

  /// CHECK-START: int Main.$opt$noinline$notXorToXor(int, int) instruction_simplifier (after)
  /// CHECK-DAG:       <<P1:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<Xor:i\d+>>         Xor [<<P1>>,<<P2>>]
  /// CHECK-DAG:                            Return [<<Xor>>]

  /// CHECK-START: int Main.$opt$noinline$notXorToXor(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                            Not

  public static int $opt$noinline$notXorToXor(int a, int b) {
    if (doThrow) throw new Error();
    return ~a ^ ~b;
  }

  /**
   * Test transformation of Not/Not/Xor into Xor for boolean negations.
   * Note that the graph before this instruction simplification pass does not
   * contain `HBooleanNot` instructions. This is because this transformation
   * follows the optimization of `HSelect` to `HBooleanNot` occurring in the
   * same pass.
   */

  /// CHECK-START: boolean Main.$opt$noinline$booleanNotXorToXor(boolean, boolean) instruction_simplifier$after_bce (after)
  /// CHECK-NOT:                            BooleanNot

  public static boolean $opt$noinline$booleanNotXorToXor(boolean a, boolean b) {
    if (doThrow) throw new Error();
    return !a ^ !b;
  }

  /**
   * Check that no transformation is done when one Not has multiple uses.
   */

  /// CHECK-START: int Main.$opt$noinline$notMultipleUses(int, int) instruction_simplifier (before)
  /// CHECK-DAG:       <<P1:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<And1:i\d+>>        And [{{i\d+}},{{i\d+}}]
  /// CHECK-DAG:       <<And2:i\d+>>        And [{{i\d+}},{{i\d+}}]
  /// CHECK-DAG:       <<Add:i\d+>>         Add [<<And1>>,<<And2>>]
  /// CHECK-DAG:                            Return [<<Add>>]

  /// CHECK-START: int Main.$opt$noinline$notMultipleUses(int, int) instruction_simplifier (after)
  /// CHECK-DAG:       <<P1:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<P2:i\d+>>          ParameterValue
  /// CHECK-DAG:       <<One:i\d+>>         IntConstant 1
  /// CHECK-DAG:       <<Not2:i\d+>>        Not [<<P2>>]
  /// CHECK-DAG:       <<And2:i\d+>>        And [<<Not2>>,<<One>>]
  /// CHECK-DAG:       <<Not1:i\d+>>        Not [<<P1>>]
  /// CHECK-DAG:       <<And1:i\d+>>        And [<<Not1>>,<<Not2>>]
  /// CHECK-DAG:       <<Add:i\d+>>         Add [<<And2>>,<<And1>>]
  /// CHECK-DAG:                            Return [<<Add>>]

  /// CHECK-START: int Main.$opt$noinline$notMultipleUses(int, int) instruction_simplifier (after)
  /// CHECK-NOT:                            Or

  public static int $opt$noinline$notMultipleUses(int a, int b) {
    if (doThrow) throw new Error();
    int tmp = ~b;
    return (tmp & 0x1) + (~a & tmp);
  }

  public static void main(String[] args) {
    assertIntEquals(~0xff, $opt$noinline$andToOr(0xf, 0xff));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$andToOrUsingNot",
        int.class, 0xf, 0xff));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$andToOrUsingXor",
        int.class, 0xf, 0xff));
    assertEquals(true, $opt$noinline$booleanAndToOr(false, false));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanAndToOrWithBranch",
        boolean.class, false, false));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanAndToOrWithXor",
        boolean.class, false, false));
    assertLongEquals(~0xf, $opt$noinline$orToAnd(0xf, 0xff));
    assertLongEquals(~0xf, $noinline$runSmaliTest("$opt$noinline$orToAndWithNot",
        long.class, 0xfL, 0xffL));
    assertLongEquals(~0xf, $noinline$runSmaliTest("$opt$noinline$orToAndWithXor",
        long.class, 0xfL, 0xffL));
    assertEquals(false, $opt$noinline$booleanOrToAnd(true, true));
    assertEquals(false, $noinline$runSmaliTest("$opt$noinline$booleanOrToAndWithBranch",
        boolean.class, true, true));
    assertEquals(false, $noinline$runSmaliTest("$opt$noinline$booleanOrToAndWithXor",
        boolean.class, true, true));
    assertIntEquals(-1, $opt$noinline$regressInputsAway(0xf, 0xff));
    assertIntEquals(-1, $noinline$runSmaliTest("$opt$noinline$regressInputsAwayWithNot",
        int.class, 0xf, 0xff));
    assertIntEquals(-1, $noinline$runSmaliTest("$opt$noinline$regressInputsAwayWithXor",
        int.class, 0xf, 0xff));
    assertIntEquals(0xf0, $opt$noinline$notXorToXor(0xf, 0xff));
    assertIntEquals(0xf0, $noinline$runSmaliTest("$opt$noinline$notXorToXorWithNot",
        int.class, 0xf, 0xff));
    assertIntEquals(0xf0, $noinline$runSmaliTest("$opt$noinline$notXorToXorWithXor",
        int.class, 0xf, 0xff));
    assertEquals(true, $opt$noinline$booleanNotXorToXor(true, false));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanNotXorToXorWithBranch",
        boolean.class, true, false));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanNotXorToXorWithXor",
        boolean.class, true, false));
    assertIntEquals(~0xff, $opt$noinline$notMultipleUses(0xf, 0xff));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$notMultipleUsesWithNot",
        int.class, 0xf, 0xff));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$notMultipleUsesWithXor",
        int.class, 0xf, 0xff));
  }
}
