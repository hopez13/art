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

package other;

/**
 * Tests for dot product idiom vectorization.
 */
public class TestVarious {

  /// CHECK-START: int other.TestVarious.testDotProdConstRight(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const89:i\d+>> IntConstant 89                                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Const89>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestVarious.testDotProdConstRight(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Const89:i\d+>> IntConstant 89                                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const89>>]                      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Repl>>] type:Int8    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdConstRight(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp =  b[i] * 89;
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int other.TestVarious.testDotProdConstLeft(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const89:i\d+>> IntConstant 89                                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Const89>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestVarious.testDotProdConstLeft(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Const89:i\d+>> IntConstant 89                                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const89>>]                      loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Repl>>] type:Uint8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdConstLeft(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = 89 * (b[i] & 0xff);
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int other.TestVarious.testDotProdLoopInvariantConvRight(byte[], byte[], int) loop_optimization (before)
  /// CHECK-DAG: <<Param:i\d+>>   ParameterValue                                        loop:none
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<ConstL:i\d+>>  IntConstant 129                                       loop:none
  /// CHECK-DAG: <<AddP:i\d+>>    Add [<<Param>>,<<ConstL>>]                            loop:none
  /// CHECK-DAG: <<TypeCnv:b\d+>> TypeConversion [<<AddP>>]                             loop:none
  //
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<TypeCnv>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestVarious.testDotProdLoopInvariantConvRight(byte[], byte[], int) loop_optimization (after)
  /// CHECK-DAG: <<Param:i\d+>>   ParameterValue                                        loop:none
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<ConstL:i\d+>>  IntConstant 129                                       loop:none
  /// CHECK-DAG: <<AddP:i\d+>>    Add [<<Param>>,<<ConstL>>]                            loop:none
  /// CHECK-DAG: <<TypeCnv:b\d+>> TypeConversion [<<AddP>>]                             loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<TypeCnv>>]                      loop:none
  //
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Repl>>] type:Int8    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdLoopInvariantConvRight(byte[] a, byte[] b, int param) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * ((byte)(param + 129));
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int other.TestVarious.testDotProdByteToChar(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdByteToChar(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)((byte)(a[i] + 129))) * b[i];
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int other.TestVarious.testDotProdMixedSize(byte[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdMixedSize(byte[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int other.TestVarious.testDotProdMixedSizeAndSign(byte[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdMixedSizeAndSign(byte[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int other.TestVarious.testDotProdInt32(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestVarious.testDotProdInt32(int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:d\d+>>     VecMul [<<Load1>>,<<Load2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecAdd [<<Phi2>>,<<Mul>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                      loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]             loop:none
  public static final int testDotProdInt32(int[] a, int[] b) {
    int s = 1;
    for (int i = 0;  i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s;
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void run() {
    final short MAX_S = Short.MAX_VALUE;
    final short MIN_S = Short.MAX_VALUE;

    byte[] b1 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    byte[] b2 = {  127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  127,  127,  127,  127 };

    char[] c1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[] c2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };

    int[] i1 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    int[] i2 = {  127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  127,  127,  127,  127 };

    short[] s1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };

    expectEquals(56516, testDotProdConstRight(b1, b2));
    expectEquals(56516, testDotProdConstLeft(b1, b2));
    expectEquals(-1279, testDotProdLoopInvariantConvRight(b1, b2, 129));
    expectEquals(-8519423, testDotProdByteToChar(c1, c2));
    expectEquals(-8388351, testDotProdMixedSize(b1, s1));
    expectEquals(-8388351, testDotProdMixedSizeAndSign(b1, c2));
    expectEquals(-81279, testDotProdInt32(i1, i2));
  }

  public static void main(String[] args) {
    run();
  }
}
