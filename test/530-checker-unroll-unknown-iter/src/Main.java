/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Simple tests for dynamic unrolling (partial loop unroll with unknown iterations)
// This optimization makes use of loop versioning, which generates version
// (i.e. copy) of original loop
// This optimization append copy loop at exit of original loop, unroll the copy
// loop by specific unroll factor and then modify the iterations of both original
// (orig_iter % unroll_factor) as well as copy loop (orig_iter / unroll_factor)
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
  int id;
  public Main() {
  }

  // Check to verify that loop has been dynamically unrolled.
  /// CHECK-START: int Main.testArraySumDown(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant -1                            loop:none
  /// CHECK-DAG: <<Nullchk:l\d+>> NullCheck [<<Array>>]                     loop:none
  /// CHECK-DAG: <<Len:i\d+>>     ArrayLength                               loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi                                       loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi                                       loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckA:z\d+>>  LessThan [<<Phi2>>,<<Const0>>]            loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>     If [<<CheckA>>]                           loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>    ArrayGet [<<Nullchk>>,<<Phi2>>]             loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddA:i\d+>>    Add [<<Phi1>>,<<GetA>>]                   loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddB:i\d+>>    Add [<<Phi2>>,<<Const1>>]                 loop:<<LoopA>>      outer_loop:none
  //
  /// CHECK-START: int Main.testArraySumDown(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const3:i\d+>>  IntConstant -1                            loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Const2:i\d+>>  IntConstant 2                             loop:none
  /// CHECK-DAG: <<Nullchk:l\d+>> NullCheck [<<Array>>]                     loop:none
  /// CHECK-DAG: <<Len:i\d+>>     ArrayLength                               loop:none
  /// CHECK-DAG: <<Sel1:i\d+>>    Select                                    loop:none
  /// CHECK-DAG: <<Rem1:i\d+>>    Rem [<<Sel1>>,<<Const2>>]                 loop:none
  /// CHECK-DAG: <<Shr1:i\d+>>    Shr [<<Sel1>>,<<Const1>>]                 loop:none
  //
  // Regular loop with updated iterations (orig_iter % unroll_factor)
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi                                       loop:<<LoopA:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi                                       loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>    Phi                                       loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<CheckB:z\d+>>  GreaterThanOrEqual [<<Phi3>>,<<Rem1>>]    loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<IfB:v\d+>>     If [<<CheckB>>]                           loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<GetA:i\d+>>    ArrayGet [<<Nullchk>>,<<Phi2>>]             loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddA:i\d+>>    Add [<<Phi1>>,<<GetA>>]                   loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddB:i\d+>>    Add [<<Phi2>>,<<Const3>>]                 loop:<<LoopA>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Phi3>>,<<Const1>>]                 loop:<<LoopA>>      outer_loop:none
  //
  // Unrolled loop with updated iterations (orig_iter / unroll_factor)
  /// CHECK-DAG: <<Phi4:i\d+>>    Phi                                       loop:<<LoopB:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi5:i\d+>>    Phi                                       loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<Phi6:i\d+>>    Phi                                       loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<CheckC:z\d+>>  GreaterThanOrEqual [<<Phi6>>,<<Shr1>>]    loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<IfC:v\d+>>     If [<<CheckC>>]                           loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetB:i\d+>>    ArrayGet [<<Nullchk>>,<<Phi5>>]             loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddD:i\d+>>    Add [<<Phi4>>,<<GetB>>]                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddE:i\d+>>    Add [<<Phi5>>,<<Const3>>]                 loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<GetC:i\d+>>    ArrayGet [<<Nullchk>>,<<AddE>>]             loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddF:i\d+>>    Add [<<AddD>>,<<GetC>>]                   loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddG:i\d+>>    Add [<<AddE>>,<<Const3>>]                 loop:<<LoopB>>      outer_loop:none
  /// CHECK-DAG: <<AddH:i\d+>>    Add [<<Phi6>>,<<Const1>>]                 loop:<<LoopB>>      outer_loop:none
  private static int testArraySumDown(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      result += x[i];
    }
    return result;
  }

  // This loop will be dynamically unrolled.
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
  private static void testArrayInitialize(int[] array) {
    int n = array.length;
    for (int i = 0; i < n; i++) {
       array[i] = (n % 2) + i;
    }
  }

  // This loop will be dynamically unrolled.
  private static int testHashCode(int[] x) {
    int hash = 0;
    for (int i = 0; i < x.length; i++) {
       hash = 31 * hash + x[i];
    }
    return hash;
  }

  // This loop will be dynamically unrolled.
  private static void testFloatInitialize(float[] array) {
    int n = array.length;
    array[0] = 99;
    for (int i = 1; i < n; i++) {
      array[i] = array[i-1] + i;
    }
  }

  private static float testFloatCode(float[] x) {
    int n = x.length;
    float result=0, result1=1, result2=2, result3=3 ;

    for (int i = 0; i < n; ) {
      result += x[i++];
      result1 += x[i++] + 1.5;
      result2 += x[i++] + 2.5;
      result3 += x[i++] + 3.5;
    }

    return result + result1 + result2 + result3;
  }

  // This loop will be dynamically unrolled.
  public static int periodicIdiom(int tc) {
    int[] x = { 1, 3 };
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
  public int testCheckSum(int[] arr) {
    byte sum = 0;
    byte xor = 0;
    for (int i = 0; i < arr.length; i++) {
      sum += arr[i];
      xor |= arr[i];
    }
    return (xor << 8) + sum;
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

  public void run(int N) {
    int[] iarray = new int[N];
    testArrayInitialize(iarray);
    expectEquals(4950, testArraySumDown(iarray));
    expectEquals(58926130, testHashCode(iarray));

    int rr[] = new int[N];
    for (int i = 0; i < N; i++) {
      rr[i] = iarray[i];
    }
    expectEquals(4950, testArraySumByTwo(rr));
    expectEquals(32598, testCheckSum(rr));

    float[] farray = new float[N];
    testFloatInitialize(farray);
    expectEquals(176743.5, testFloatCode(farray));
  }

  // Simple Test routine.
  public static void main(String args[]){
    Main plu = new Main();
    plu.run(100);
    System.out.println("passed");
  }
}
