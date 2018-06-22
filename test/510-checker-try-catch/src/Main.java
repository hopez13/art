/*
 * Copyright (C) 2015 The Android Open Source Project
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

  // Workaround for b/18051191.
  class InnerClass {}

  public enum TestPath {
    ExceptionalFlow1(true, false, 3),
    ExceptionalFlow2(false, true, 8),
    NormalFlow(false, false, 42);

    TestPath(boolean arg1, boolean arg2, int expected) {
      this.arg1 = arg1;
      this.arg2 = arg2;
      this.expected = expected;
    }

    public boolean arg1;
    public boolean arg2;
    public int expected;
  }

  // Test that IntermediateAddress instruction is not alive across BoundsCheck which can throw to
  // a catch block.
  //
  /// CHECK-START-{ARM,ARM64}: void Main.testBoundsCheckAndCatch(int, int[]) GVN$after_arch (before)
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Offset:i\d+>>       IntConstant 12
  /// CHECK-DAG: <<IndexParam:i\d+>>   ParameterValue
  /// CHECK-DAG: <<Array:l\d+>>        ParameterValue
  //
  /// CHECK-DAG: <<Xplus1:i\d+>>       Add [<<IndexParam>>,<<Const1>>]
  /// CHECK-DAG: <<NullCh1:l\d+>>      NullCheck [<<Array>>]
  /// CHECK-DAG: <<Length:i\d+>>       ArrayLength
  /// CHECK-DAG: <<BoundsCh1:i\d+>>    BoundsCheck [<<Xplus1>>,<<Length>>]
  /// CHECK-DAG: <<IntAddr1:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh1>>,<<Const1>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG: <<IntAddr2:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr2>>,<<BoundsCh1>>,<<Const2>>]
  /// CHECK-DAG: <<BoundsCh2:i\d+>>    BoundsCheck
  /// CHECK-DAG: <<IntAddr3:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr3>>,<<BoundsCh2>>,<<Const1>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG:                       LoadException
  /// CHECK-DAG:                       ClearException
  /// CHECK-DAG: <<BoundsCh3:i\d+>>    BoundsCheck
  /// CHECK-DAG: <<IntAddr4:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr4>>,<<BoundsCh3>>,<<Const1>>]
  //
  /// CHECK-NOT:                       NullCheck
  /// CHECK-NOT:                       IntermediateAddress

  /// CHECK-START-{ARM,ARM64}: void Main.testBoundsCheckAndCatch(int, int[]) GVN$after_arch (after)
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Offset:i\d+>>       IntConstant 12
  /// CHECK-DAG: <<IndexParam:i\d+>>   ParameterValue
  /// CHECK-DAG: <<Array:l\d+>>        ParameterValue
  //
  /// CHECK-DAG: <<Xplus1:i\d+>>       Add [<<IndexParam>>,<<Const1>>]
  /// CHECK-DAG: <<NullCh1:l\d+>>      NullCheck [<<Array>>]
  /// CHECK-DAG: <<Length:i\d+>>       ArrayLength
  /// CHECK-DAG: <<BoundsCh1:i\d+>>    BoundsCheck [<<Xplus1>>,<<Length>>]
  /// CHECK-DAG: <<IntAddr1:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh1>>,<<Const1>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh1>>,<<Const2>>]
  /// CHECK-DAG: <<BoundsCh2:i\d+>>    BoundsCheck
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh2>>,<<Const1>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG:                       LoadException
  /// CHECK-DAG:                       ClearException
  /// CHECK-DAG: <<BoundsCh3:i\d+>>    BoundsCheck
  /// CHECK-DAG: <<IntAddr4:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr4>>,<<BoundsCh3>>,<<Const1>>]
  //
  /// CHECK-NOT:                       NullCheck
  /// CHECK-NOT:                       IntermediateAddress
  public static void testBoundsCheckAndCatch(int x, int[] a) {
    a[x + 1] = 1;
    try {
      a[x + 1] = 2;
      a[x + 2] = 1;
    } catch (Exception e) {
      a[x] = 1;
    }
  }

  public static void testDivZeroCheckAndCatch(int x, int y, int[] a) {
    a[x + 1] = 1;
    try {
      a[x + 1] = 1 / y;
    } catch (Exception e) {
      a[x] = 1;
    }
  }

  public static void testMethod(String method) throws Exception {
    Class<?> c = Class.forName("Runtime");
    Method m = c.getMethod(method, boolean.class, boolean.class);

    for (TestPath path : TestPath.values()) {
      Object[] arguments = new Object[] { path.arg1, path.arg2 };
      int actual = (Integer) m.invoke(null, arguments);

      if (actual != path.expected) {
        throw new Error("Method: \"" + method + "\", path: " + path + ", " +
                        "expected: " + path.expected + ", actual: " + actual);
      }
    }
  }

  public static void main(String[] args) throws Exception {
    testMethod("testUseAfterCatch_int");
    testMethod("testUseAfterCatch_long");
    testMethod("testUseAfterCatch_float");
    testMethod("testUseAfterCatch_double");
    testMethod("testCatchPhi_const");
    testMethod("testCatchPhi_int");
    testMethod("testCatchPhi_long");
    testMethod("testCatchPhi_float");
    testMethod("testCatchPhi_double");
    testMethod("testCatchPhi_singleSlot");
    testMethod("testCatchPhi_doubleSlot");
  }
}
