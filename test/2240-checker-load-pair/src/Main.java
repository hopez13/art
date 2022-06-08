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

  private static volatile int result1 = 0;
  private static volatile int result2 = 0;

  public static void $noinline$foo(int[] arr) {
    arr[0] = 0;
    arr[1] = 1;
  }

  // Test 1: Checks that two consecutive ArrayGet instructions combine into a
  // single ArmLoadPair instruction.

  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGets(int[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:      <<ArrGet0:i\d+>>  ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK-DAG:  <<Class:l\d+>>    LoadClass
  /// CHECK:                        StaticFieldSet  [<<Class>>,<<ArrGet0>>]
  /// CHECK:      <<ArrGet1:i\d+>>  ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK:                        StaticFieldSet  [<<Class>>,<<ArrGet1>>]
  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGets(int[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:      <<Ldp:i\d+>>      ArmLoadPair     [<<Check>>,<<Const0>>]
  /// CHECK:      <<Proj0:i\d+>>    ProjectionNode  [<<Ldp>>,<<Const0>>]
  /// CHECK-DAG:  <<Class:l\d+>>    LoadClass
  /// CHECK:                        StaticFieldSet  [<<Class>>,<<Proj0>>]
  /// CHECK:      <<Proj1:i\d+>>    ProjectionNode  [<<Ldp>>,<<Const1>>]
  /// CHECK:                        StaticFieldSet  [<<Class>>,<<Proj1>>]

  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGets(int[]) disassembly (after)
  /// CHECK:        ldp {{w\d+}}, {{w\d+}},

  public static void $noinline$testConsecutiveArrayGets(int[] a) {
    result1 = a[0];
    result2 = a[1];
  }

  // Test 2: Check that two non-consecutive ArrayGet instructions do not
  // combine into a single ArmLoadPair instruction.

  /// CHECK-START-ARM64:  void Main.$noinline$testNonConsecutiveArrayGets(int[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const2:i\d+>>   IntConstant     2
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const2>>]
  /// CHECK-START-ARM64:  void Main.$noinline$testNonConsecutiveArrayGets(int[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const2:i\d+>>   IntConstant     2
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const2>>]
  /// CHECK-NOT:                    ArmLoadPair     [<<Check>>,<<Const0>>]

  public static void $noinline$testNonConsecutiveArrayGets(int[] a) {
    result1 = a[0];
    result2 = a[2];
  }

  // Test 3: Check that two consecutive ArrayGet instructions do not combine
  // when the second element is set before the second ArrayGet.

  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithSetBeforeGet(int[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithSetBeforeGet(int[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK-NOT:                    ArmLoadPair     [<<Check>>,<<Const0>>]

  public static void $noinline$testConsecutiveArrayGetsWithSetBeforeGet(int[] a) {
    result1 = a[0];
    a[1] = 5;
    result2 = a[1];
  }

  // Test 4: Check that two consecutive ArrayGet instructions do combine when
  // the first element is set before the second ArrayGet.

  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithFirstSetBeforeGet(int[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithFirstSetBeforeGet(int[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK-NOT:                    ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK-NOT:                    ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK:      <<Ldp:i\d+>>      ArmLoadPair     [<<Check>>,<<Const0>>]
  /// CHECK:                        ProjectionNode  [<<Ldp>>,<<Const0>>]
  /// CHECK:                        ProjectionNode  [<<Ldp>>,<<Const1>>]

  public static void $noinline$testConsecutiveArrayGetsWithFirstSetBeforeGet(int[] a) {
    result1 = a[0];
    a[0] = 5;
    result2 = a[1];
  }

  // Test 5: Check that two consecutive ArrayGet instructions do combine when
  // an unrelated array element is set before the second ArrayGet.

  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithUnrelatedSetBeforeGet(int[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithUnrelatedSetBeforeGet(int[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK-NOT:                    ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK-NOT:                    ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK:      <<Ldp:i\d+>>      ArmLoadPair     [<<Check>>,<<Const0>>]
  /// CHECK:                        ProjectionNode  [<<Ldp>>,<<Const0>>]
  /// CHECK:                        ProjectionNode  [<<Ldp>>,<<Const1>>]

  public static void $noinline$testConsecutiveArrayGetsWithUnrelatedSetBeforeGet(int[] a) {
    result1 = a[0];
    a[5] = 5;
    result2 = a[1];
  }

  // Test 6: Check that two consecutive ArrayGet instructions do not combine
  // when a method is invoked before the second ArrayGet.

  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithCall(int[]) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK-START-ARM64:  void Main.$noinline$testConsecutiveArrayGetsWithCall(int[]) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:  <<Arr:l\d+>>      ParameterValue
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant     0
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant     1
  /// CHECK:      <<Check:l\d+>>    NullCheck       [<<Arr>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const0>>]
  /// CHECK:                        ArrayGet        [<<Check>>,<<Const1>>]
  /// CHECK-NOT:                    ArmLoadPair     [<<Check>>,<<Const0>>]

  public static void $noinline$testConsecutiveArrayGetsWithCall(int[] a) {
    result1 = a[0];
    $noinline$foo(a);
    result2 = a[1];
  }

  public static void main(String[] args) {
    int[] a = new int[100];
    $noinline$testConsecutiveArrayGets(a);
    $noinline$testNonConsecutiveArrayGets(a);
    $noinline$testConsecutiveArrayGetsWithSetBeforeGet(a);
    $noinline$testConsecutiveArrayGetsWithFirstSetBeforeGet(a);
    $noinline$testConsecutiveArrayGetsWithUnrelatedSetBeforeGet(a);
    $noinline$testConsecutiveArrayGetsWithCall(a);
  }
}
