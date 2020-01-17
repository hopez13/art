/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

public class Main {

  static boolean result;
  static int intResult;
  static final int kSingleBitMask=0x4;
  static final int kMultiBitMask=0x5;
  static final long kLongSingleBitMask=0x4;

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZero(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:false bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          Equal
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZero(int) disassembly (after)
  /// CHECK:            tbz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitSetWithZero(int n){
    if((n & kSingleBitMask) != 0) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZero(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:true bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          NotEqual
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZero(int) disassembly (after)
  /// CHECK:            tbnz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitNotSetWithZero(int n){
    if((n & kSingleBitMask) == 0) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMask(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Mask>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMask(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:false bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMask(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          NotEqual
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMask(int) disassembly (after)
  /// CHECK:            tbz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitSetWithMask(int n){
    if((n & kSingleBitMask) == kSingleBitMask) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMask(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Mask>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMask(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:true bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMask(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          Equal
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMask(int) disassembly (after)
  /// CHECK:            tbnz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitNotSetWithMask(int n){
    if((n & kSingleBitMask) != kSingleBitMask) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroLong(long) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:  <<Mask:j\d+>>   LongConstant 4
  /// CHECK:  <<Zero:j\d+>>   LongConstant 0
  /// CHECK:  <<And:j\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroLong(long) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:false bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroLong(long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          Equal
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroLong(long) disassembly (after)
  /// CHECK:            tbz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}


   public static void testSingleBitSetWithZeroLong(long n){
    if((n & kLongSingleBitMask) != 0) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroLong(long) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:  <<Mask:j\d+>>   LongConstant 4
  /// CHECK:  <<Zero:j\d+>>   LongConstant 0
  /// CHECK:  <<And:j\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroLong(long) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:true bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroLong(long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          NotEqual
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroLong(long) disassembly (after)
  /// CHECK:            tbnz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitNotSetWithZeroLong(long n){
    if((n & kLongSingleBitMask) == 0) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMaskLong(long) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:  <<Mask:j\d+>>   LongConstant 4
  /// CHECK:  <<And:j\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Mask>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMaskLong(long) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:false bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMaskLong(long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          NotEqual
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithMaskLong(long) disassembly (after)
  /// CHECK:            tbz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitSetWithMaskLong(long n){
    if((n & kLongSingleBitMask) == kLongSingleBitMask) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMaskLong(long) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:  <<Mask:j\d+>>   LongConstant 4
  /// CHECK:  <<And:j\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Mask>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMaskLong(long) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:j\d+>>    ParameterValue
  /// CHECK:            TestBitAndBranch [<<Value>>] branch_if_bit_set:true bit_to_test:2

  /*
   *  Check the instructions are replaced and removed
   */

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMaskLong(long) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          And
  /// CHECK-NOT:          Equal
  /// CHECK-NOT:          If

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithMaskLong(long) disassembly (after)
  /// CHECK:            tbnz w{{\d+}}, #2, #+0x{{[0-9A-Fa-f]+}}

  public static void testSingleBitNotSetWithMaskLong(long n){
    if((n & kLongSingleBitMask) != kLongSingleBitMask) {
      result=true;
    } else {
      result=false;
    }
  }


  /// CHECK-START-ARM64: void Main.testMultiBitSetWithZero(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 5
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testMultiBitSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 5
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testMultiBitSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          TestBitAndBranch

  public static void testMultiBitSetWithZero(int n){
    if((n & kMultiBitMask) != 0) {
      result=true;
    } else {
      result=false;
    }
  }


  /// CHECK-START-ARM64: void Main.testMultiBitNotSetWithZero(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 5
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testMultiBitNotSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 5
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testMultiBitNotSetWithZero(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          TestBitAndBranch

  public static void testMultiBitNotSetWithZero(int n){
    if((n & kMultiBitMask) == 0) {
      result = true;
    } else {
      result = false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithOne(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<One:i\d+>>    IntConstant 1
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<One>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithOne(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<One:i\d+>>    IntConstant 1
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<One>>]
  /// CHECK:            If [<<Cond>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithOne(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          TestBitAndBranch

  public static void testSingleBitSetWithOne(int n){
    if((n & kSingleBitMask) == 0x1) {
      result=true;
    } else {
      result=false;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroAndReused(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<ResultMask:i\d+>> IntConstant 5
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]
  /// CHECK:            And [<<And>>,<<ResultMask>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroAndReused(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<ResultMask:i\d+>> IntConstant 5
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   Equal [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]
  /// CHECK:            And [<<And>>,<<ResultMask>>]

  /// CHECK-START-ARM64: void Main.testSingleBitSetWithZeroAndReused(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          TestBitAndBranch

  public static void testSingleBitSetWithZeroAndReused(int n){
    int andResult = n & kSingleBitMask;
    if(andResult != 0) {
      intResult=andResult & kMultiBitMask;
    } else {
      intResult=0;
    }
  }

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroAndReused(int) instruction_simplifier_arm64 (before)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<ResultMask:i\d+>> IntConstant 5
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]
  /// CHECK:            And [<<And>>,<<ResultMask>>]

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroAndReused(int) instruction_simplifier_arm64 (after)
  /// CHECK:  <<Value:i\d+>>    ParameterValue
  /// CHECK:  <<Mask:i\d+>>   IntConstant 4
  /// CHECK:  <<Zero:i\d+>>   IntConstant 0
  /// CHECK:  <<ResultMask:i\d+>> IntConstant 5
  /// CHECK:  <<And:i\d+>>    And [<<Value>>,<<Mask>>]
  /// CHECK:  <<Cond:z\d+>>   NotEqual [<<And>>,<<Zero>>]
  /// CHECK:            If [<<Cond>>]
  /// CHECK:            And [<<And>>,<<ResultMask>>]

  /// CHECK-START-ARM64: void Main.testSingleBitNotSetWithZeroAndReused(int) instruction_simplifier_arm64 (after)
  /// CHECK-NOT:          TestBitAndBranch


  public static void testSingleBitNotSetWithZeroAndReused(int n){
    int andResult = n & kSingleBitMask;
    if(andResult == 0) {
      intResult=0;
    } else {
      intResult=andResult & kMultiBitMask;
    }
  }

  public static void main(String args[]) {
    // Test combine occurs when the bit mask only has a single bit set
    // and that the branch is still followed correctly
    //
    // The TBZ and TBNZ instructions are used to test if a bit is set and
    // branch on this condition this can be done without the TBZ instruction
    // using the following instructions:
    // And
    // Equals / NotEquals
    // If
    // The And is done against a mask with the only the bit of interest set.

    // To check if a bit is set (which is what the TBNZ instruction is used for)
    // after the And we can either check NotEquals 0 or Equals Mask. The first
    // set of tests checks under these conditions the TBNZ instruction is emitted
    // and the branching is still correct.
    testSingleBitSetWithZero(kSingleBitMask * 2);
    assert(!result);
    testSingleBitSetWithZero(kSingleBitMask);
    assert(result);
    testSingleBitSetWithZero(kSingleBitMask + 1);
    assert(result);
    testSingleBitSetWithMask(kSingleBitMask * 2);
    assert(!result);
    testSingleBitSetWithMask(kSingleBitMask);
    assert(result);
    testSingleBitSetWithMask(kSingleBitMask + 1);
    assert(result);
    testSingleBitSetWithZeroLong(kLongSingleBitMask * 2);
    assert(!result);
    testSingleBitSetWithZeroLong(kLongSingleBitMask);
    assert(result);
    testSingleBitSetWithZeroLong(kLongSingleBitMask + 1);
    assert(result);
    testSingleBitSetWithMaskLong(kLongSingleBitMask * 2);
    assert(!result);
    testSingleBitSetWithMaskLong(kLongSingleBitMask);
    assert(result);
    testSingleBitSetWithMaskLong(kLongSingleBitMask + 1);
    assert(result);

    // To check if a bit is not set (which the TBZ instruction is used for)
    // after the And we can check wither Equals 0 or NotEquals Mask. This set of
    // tests checks that under these conditions the TBZ instruction is emitted
    // and the branching is still correct.
    testSingleBitNotSetWithZero(kSingleBitMask * 2);
    assert(result);
    testSingleBitNotSetWithZero(kSingleBitMask);
    assert(!result);
    testSingleBitNotSetWithMask(kSingleBitMask * 2);
    assert(result);
    testSingleBitNotSetWithMask(kSingleBitMask);
    assert(!result);
    testSingleBitNotSetWithZeroLong(kLongSingleBitMask * 2);
    assert(result);
    testSingleBitNotSetWithZeroLong(kLongSingleBitMask);
    assert(!result);
    testSingleBitNotSetWithMaskLong(kLongSingleBitMask * 2);
    assert(result);
    testSingleBitNotSetWithMaskLong(kLongSingleBitMask);
    assert(!result);

    // The remaining tests check that if the above conditions are not met no instruction
    // simplification occurs
    testMultiBitSetWithZero(kMultiBitMask * 2);
    assert(!result);
    testMultiBitSetWithZero(kMultiBitMask);
    assert(result);
    testMultiBitNotSetWithZero(kMultiBitMask * 2);
    assert(result);
    testMultiBitNotSetWithZero(kMultiBitMask);
    assert(!result);
    testSingleBitSetWithOne(kSingleBitMask * 2);
    assert(!result);
    // At the moment the optimization only occurs when each instruction only has one use
    // check this is the case
    testSingleBitSetWithZeroAndReused(kSingleBitMask);
    assertIntEquals(kSingleBitMask, intResult);
    testSingleBitSetWithZeroAndReused(kSingleBitMask * 2);
    assertIntEquals(0, intResult);
    testSingleBitNotSetWithZeroAndReused(kSingleBitMask);
    assertIntEquals(kSingleBitMask, intResult);
    testSingleBitNotSetWithZeroAndReused(kSingleBitMask * 2);
    assertIntEquals(0, intResult);
  }
}
