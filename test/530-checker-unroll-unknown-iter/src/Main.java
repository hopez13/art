/*
 * Copyright (C) 2020 The Android Open Source Project
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

// Simple tests for dynamic unrolling (partial loop unroll with unknown iterations).
// This optimization makes use of loop versioning, which generates version
// (i.e. copy) of original loop.
// This optimization appends copy loop at exit of original loop, unroll the copy
// loop by specific unroll factor and then modify the iterations of both original
// (orig_iter % unroll_factor) as well as copy loop (orig_iter / unroll_factor).
//
//       Before               After(loop versioning)        After(dynamic unrolling)
//
//         |B|                      |B|                         |B|
//          |                        |                           |
//          v                        v                           v
//         |1|                      |1|_________                |1|
//          |                        |          |                |
//          v                        v          v                v
//         |2|<-\                   |2|<-\     |2A|<-\       /->|2|
//         / \  /                   / \  /     /  \  /       \  / \
//        v   v/                   |   v/      |   v/         \v   \
//        |   |3|                  |  |3|      | |3A|         |3|   v
//        |                        | __________|                 /->|2A|
//        |                        ||                            \  / \
//        v                        vv                             \v   \
//       |4|                       |4|                           |3A|   v
//        |                         |                                  |4|
//        v                         v                                   |
//       |E|                       |E|                                  v
//                                                                     |E|
public class Main {

  // Check to verify that loop has been dynamically unrolled.
  // PhiR is reduction PHI and PhiI is induction PHI
  /// CHECK-START: int Main.testArraySumDown(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>     ParameterValue                             loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                              loop:none
  /// CHECK-DAG: <<ConstN1:i\d+>>   IntConstant -1                             loop:none
  /// CHECK-DAG: <<Nullchk:l\d+>>   NullCheck [<<Array>>]                      loop:none
  /// CHECK-DAG: <<Len:i\d+>>       ArrayLength                                loop:none
  /// CHECK-DAG: <<PhiR:i\d+>>      Phi                                        loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>      Phi                                        loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckA:z\d+>>    LessThan [<<PhiI>>,<<Const0>>]             loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>       If [<<CheckA>>]                            loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>      ArrayGet [<<Nullchk>>,<<PhiI>>]            loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddA:i\d+>>      Add [<<PhiR>>,<<GetA>>]                    loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddB:i\d+>>      Add [<<PhiI>>,<<ConstN1>>]                 loop:<<LoopA>>      outer_loop:none
  //
  /// CHECK-START: int Main.testArraySumDown(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>     ParameterValue                             loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                              loop:none
  /// CHECK-DAG: <<ConstN1:i\d+>>   IntConstant -1                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                              loop:none
  /// CHECK-DAG: <<Const2:i\d+>>    IntConstant 2                              loop:none
  /// CHECK-DAG: <<Nullchk:l\d+>>   NullCheck [<<Array>>]                      loop:none
  /// CHECK-DAG: <<Len:i\d+>>       ArrayLength                                loop:none
  /// CHECK-DAG: <<AddL:i\d+>>      Add [<<Len>>,<<ConstN1>>]                  loop:none
  /// CHECK-DAG: <<Sel1:i\d+>>      Select                                     loop:none
  /// CHECK-DAG: <<Rem1:i\d+>>      Rem [<<Sel1>>,<<Const2>>]                  loop:none
  /// CHECK-DAG: <<Mul1:i\d+>>      Mul [<<Rem1>>,<<Const1>>]                  loop:none
  /// CHECK-DAG: <<Sub1:i\d+>>      Sub [<<AddL>>,<<Mul1>>]                    loop:none
  //
  // Regular loop with updated iterations (orig_iter % unroll_factor).
  /// CHECK-DAG: <<PhiR1:i\d+>>     Phi                                        loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI1:i\d+>>     Phi                                        loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckB:z\d+>>    LessThanOrEqual [<<PhiI1>>,<<Sub1>>]       loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfB:v\d+>>       If [<<CheckB>>]                            loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>      ArrayGet [<<Nullchk>>,<<PhiI1>>]           loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddA:i\d+>>      Add [<<PhiR1>>,<<GetA>>]                   loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddB:i\d+>>      Add [<<PhiI1>>,<<ConstN1>>]                loop:<<LoopA>>      outer_loop:none
  //
  // Unrolled loop with updated iterations (orig_iter / unroll_factor).
  /// CHECK-DAG: <<PhiR2:i\d+>>     Phi                                        loop:<<LoopB:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI2:i\d+>>     Phi                                        loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<CheckC:z\d+>>    LessThan [<<PhiI2>>,<<Const0>>]            loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<IfC:v\d+>>       If [<<CheckC>>]                            loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetB:i\d+>>      ArrayGet [<<Nullchk>>,<<PhiI2>>]           loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddD:i\d+>>      Add [<<PhiR2>>,<<GetB>>]                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddE:i\d+>>      Add [<<PhiI2>>,<<ConstN1>>]                loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>      ArrayGet [<<Nullchk>>,<<AddE>>]            loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddF:i\d+>>      Add [<<AddD>>,<<GetC>>]                    loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddG:i\d+>>      Add [<<AddE>>,<<ConstN1>>]                 loop:<<LoopB>>      outer_loop:none
  private static int testArraySumDown(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      result += x[i];
    }
    return result;
  }

  // This loop will be dynamically unrolled.
  // PhiR is reduction PHI and PhiI is induction PHI
  /// CHECK-START: int Main.testArraySumByTwo(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>     ParameterValue                             loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                              loop:none
  /// CHECK-DAG: <<Len:i\d+>>       ArrayLength                                loop:none
  /// CHECK-DAG: <<PhiR:i\d+>>      Phi                                        loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>      Phi                                        loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckA:z\d+>>    GreaterThanOrEqual                         loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>       If [<<CheckA>>]                            loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>      ArrayGet                                   loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetB:i\d+>>      ArrayGet                                   loop:<<LoopA>>      outer_loop:none
  //
  /// CHECK-START: int Main.testArraySumByTwo(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>     ParameterValue                             loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                              loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                              loop:none
  /// CHECK-DAG: <<Const2:i\d+>>    IntConstant 2                              loop:none
  /// CHECK-DAG: <<Nullchk:l\d+>>   NullCheck [<<Array>>]                      loop:none
  /// CHECK-DAG: <<Len:i\d+>>       ArrayLength                                loop:none
  /// CHECK-DAG: <<Div1:i\d+>>      Div [<<Len>>,<<Const2>>]                   loop:none
  /// CHECK-DAG: <<Sel1:i\d+>>      Select                                     loop:none
  /// CHECK-DAG: <<Rem1:i\d+>>      Rem [<<Sel1>>,<<Const2>>]                  loop:none
  /// CHECK-DAG: <<Mul1:i\d+>>      Mul [<<Rem1>>,<<Const1>>]                  loop:none
  /// CHECK-DAG: <<Add1:i\d+>>      Add [<<Const0>>,<<Mul1>>]                  loop:none
  //
  // Regular loop with updated iterations (orig_iter % unroll_factor).
  /// CHECK-DAG: <<PhiR1:i\d+>>     Phi                                        loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI1:i\d+>>     Phi                                        loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckB1:z\d+>>   GreaterThanOrEqual [<<PhiI1>>,<<Add1>>]    loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfB:v\d+>>       If [<<CheckB1>>]                           loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>      ArrayGet                                   loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetB:i\d+>>      ArrayGet                                   loop:<<LoopA>>      outer_loop:none
  //
  // Unrolled loop with updated iterations (orig_iter / unroll_factor).
  /// CHECK-DAG: <<PhiR2:i\d+>>     Phi                                        loop:<<LoopB:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI2:i\d+>>     Phi                                        loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<CheckC1:z\d+>>   GreaterThanOrEqual [<<PhiI2>>,<<Div1>>]    loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<IfC:v\d+>>       If [<<CheckC1>>]                           loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetD:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<CheckD:z\d+>>    GreaterThanOrEqual                         loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<IfD:v\d+>>       If [<<Const0>>]                            loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetE:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetF:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  //
  /// CHECK-START: int Main.testArraySumByTwo(int[]) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Array:l\d+>>     ParameterValue                             loop:none
  /// CHECK-DAG: <<Const2:i\d+>>    IntConstant 2                              loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                              loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                              loop:none
  /// CHECK-DAG: <<Nullchk:l\d+>>   NullCheck [<<Array>>]                      loop:none
  /// CHECK-DAG: <<Len:i\d+>>       ArrayLength                                loop:none
  /// CHECK-DAG: <<Div2:i\d+>>      Div [<<Len>>,<<Const2>>]                   loop:none
  /// CHECK-DAG: <<Max2:i\d+>>      Max [<<Div2>>,<<Const0>>]                  loop:none
  /// CHECK-DAG: <<Rem1:i\d+>>      Rem [<<Max2>>,<<Const2>>]                  loop:none
  //
  // Regular loop with updated iterations (orig_iter % unroll_factor).
  /// CHECK-DAG: <<PhiR1:i\d+>>     Phi                                        loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI1:i\d+>>     Phi                                        loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckB1:z\d+>>   GreaterThanOrEqual [<<PhiI1>>,<<Rem1>>]    loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfB:v\d+>>       If [<<CheckB1>>]                           loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>      ArrayGet                                   loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetB:i\d+>>      ArrayGet                                   loop:<<LoopA>>      outer_loop:none
  //
  // Unrolled loop with updated iterations (orig_iter / unroll_factor).
  /// CHECK-DAG: <<PhiR2:i\d+>>     Phi                                        loop:<<LoopB:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiI2:i\d+>>     Phi                                        loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<CheckC1:z\d+>>   GreaterThanOrEqual [<<PhiI2>>,<<Div2>>]    loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<IfC:v\d+>>       If [<<CheckC1>>]                           loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetD:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-NOT:                    GreaterThanOrEqual                         loop:<<LoopB>>      outer_loop:none
  /// CHECK-NOT:                    If [<<Const0>>]                            loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetE:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetF:i\d+>>      ArrayGet                                   loop:<<LoopB>>      outer_loop:none
  private static int testArraySumByTwo(int x[]) {
    int n = x.length / 2;
    int result = 0;
    for (int i = 0; i < n; i++) {
      int ii = i << 1;
      result += x[ii];
      result += x[ii + 1];
    }
    return result;
  }

  // This loop will be dynamically unrolled.
  private static int testHashCode(int[] x) {
    int hash = 0;
    for (int i = 0; i < x.length; i++) {
       hash = 31 * hash + x[i];
    }
    return hash;
  }

  // This loop will not be dynamically unrolled (trip count can't be generated).
  /// CHECK-START: int Main.testArraySum2(int[], int) loop_optimization (after)
  /// CHECK-NOT:  Select            loop:none
  /// CHECK-NOT:  Rem               loop:none
  /// CHECK-NOT:  Mul               loop:none
  private static int testArraySum2(int[] x, int k) {
    int result = 0;
    for (int i = 0; i < x.length; i += k) {
      result += x[i];
    }
    return result;
  }

  // This loop will not be dynamically unrolled (trip count can't be generated).
  /// CHECK-START: int Main.testGeometricInduc(int[]) loop_optimization (after)
  /// CHECK-NOT:  Select            loop:none
  /// CHECK-NOT:  Rem               loop:none
  /// CHECK-NOT:  Mul               loop:none
  private static int testGeometricInduc(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; i += (2*i + 1)) {
      result += x[i];
    }
    return result;
  }

  // This loop will not be dynamically unrolled (trip count can't be generated).
  /// CHECK-START: int Main.testPolyInduc(int[]) loop_optimization (after)
  /// CHECK-NOT:  Select            loop:none
  /// CHECK-NOT:  Rem               loop:none
  /// CHECK-NOT:  Mul               loop:none
  private static int testPolyInduc(int[] x) {
    int result = 0;
    for (int i = 0, k = 0; i < x.length; i += k) {
      result += x[i];
      k = k + 1;
    }
    return result;
  }

  // This loop will be dynamically unrolled.
  private static int periodicIdiom(int tc) {
    int[] x = {1, 3};
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];
      k = 1 - k;
    }
    return result;
  }

  // This loop will be dynamically unrolled.
  private static int testCheckSum(int[] arr) {
    byte sum = 0;
    byte xor = 0;
    for (int i = 0; i < arr.length; i++) {
      sum += arr[i];
      xor |= arr[i];
    }
    return (xor << 8) + sum;
  }

  // Helper function.
  private static void testArrayInitialize(int[] array) {
    int n = array.length;
    for (int i = 0; i < n; i++) {
      array[i] = (n % 2) + i;
    }
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // Simple Test routine.
  public static void main(String args[]){
    int size_arr = 100;

    int[] iarray = new int[size_arr];
    testArrayInitialize(iarray);
    expectEquals(4950, testArraySumDown(iarray));
    expectEquals(58926130, testHashCode(iarray));
    expectEquals(58, testGeometricInduc(iarray));
    expectEquals(455, testPolyInduc(iarray));
    expectEquals(2450, testArraySum2(iarray, 2));

    int rr[] = new int[size_arr];
    System.arraycopy(iarray, 0, rr, 0, size_arr);
    expectEquals(4950, testArraySumByTwo(rr));
    expectEquals(32598, testCheckSum(rr));

    size_arr = 103;

    int[] iarray1 = new int[size_arr];
    testArrayInitialize(iarray1);
    expectEquals(5356, testArraySumDown(iarray1));
    expectEquals(-2009024460, testHashCode(iarray1));
    expectEquals(63, testGeometricInduc(iarray1));
    expectEquals(469, testPolyInduc(iarray1));
    expectEquals(2704, testArraySum2(iarray1, 2));

    int rr1[] = new int[size_arr];
    System.arraycopy(iarray1, 0, rr1, 0, size_arr);
    expectEquals(5253, testArraySumByTwo(rr1));
    expectEquals(32492, testCheckSum(rr1));

    expectEquals(104, periodicIdiom(52));
    System.out.println("passed");
  }
}
