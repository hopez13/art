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

/**
 * Tests for dot product idiom vectorization.
 */
public class Main {

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static final int ARRAY_SIZE = 1024;

  /// CHECK-START: int Main.testDotProdConstRight(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const89:i\d+>> IntConstant 89                                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Const89>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdConstRight(byte[], byte[]) loop_optimization (after)
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

  /// CHECK-START: int Main.testDotProdConstLeft(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const89:i\d+>> IntConstant 89                                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Const89>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdConstLeft(byte[], byte[]) loop_optimization (after)
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

  /// CHECK-START: int Main.testDotProdLoopInvariantConvRight(byte[], byte[], int) loop_optimization (before)
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

  /// CHECK-START-{ARM64}: int Main.testDotProdLoopInvariantConvRight(byte[], byte[], int) loop_optimization (after)
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

  /// CHECK-START: int Main.testDotProdByteToChar(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdByteToChar(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)((byte)(a[i] + 129))) * b[i];
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int Main.testDotProdMixedSize(byte[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdMixedSize(byte[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int Main.testDotProdMixedSizeAndSign(byte[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdMixedSizeAndSign(byte[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s;
  }

  /// CHECK-START: int Main.testDotProdInt32(int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdInt32(int[], int[]) loop_optimization (after)
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

  public static void testMixedCases() {
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

  /// CHECK-START: int Main.testDotProdSimple(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdSimple(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Int8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimple(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplex(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC1:i\d+>>   Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:b\d+>>  TypeConversion [<<AddC1>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC2:i\d+>>   Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:b\d+>>  TypeConversion [<<AddC2>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplex(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplex(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)(a[i] + 1)) * ((byte)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdSimpleUnsigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Uint8  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimpleUnsigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplexUnsigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:a\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:a\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplexUnsigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint8  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (((a[i] & 0xff) + 1) & 0xff) * (((b[i] & 0xff) + 1) & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplexUnsignedCastedToSigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:b\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:b\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplexUnsignedCastedToSigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsignedCastedToSigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)((a[i] & 0xff) + 1)) * ((byte)((b[i] & 0xff) + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplexSignedCastedToUnsigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:a\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:a\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplexSignedCastedToUnsigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint8  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexSignedCastedToUnsigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((a[i] + 1) & 0xff) * ((b[i] + 1) & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdSignedWidening(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int8
  public static final int testDotProdSignedWidening(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((short)(a[i])) * ((short)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdParamSigned(byte[], byte[], int) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int8
  public static final int testDotProdParamSigned(byte[] a, byte[] b, int x) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (byte)(x) * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdParamUnsigned(byte[], byte[], int) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Uint8
  public static final int testDotProdParamUnsigned(byte[] a, byte[] b, int x) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (x & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  // No DOTPROD cases.

  /// CHECK-START: int Main.testDotProdIntParam(byte[], byte[], int) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdIntParam(byte[] a, byte[] b, int x) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = b[i] * (x);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSignedToChar(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSignedToChar(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i])) * ((char)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  // Cases when result of Mul is type-converted are not supported.

  /// CHECK-START: int Main.testDotProdSimpleCastedToSignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToSignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      byte temp = (byte)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleCastedToUnsignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToUnsignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      s += (a[i] * b[i]) & 0xff;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToSignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToSignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      byte temp = (byte)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToUnsignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToUnsignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      s += ((a[i] & 0xff) * (b[i] & 0xff)) & 0xff;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleCastedToShort(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToShort(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleCastedToChar(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToChar(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToShort(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToShort(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToChar(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToChar(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToLong(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToLong(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      long temp = (long)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdUnsignedSigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedSigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * b[i];
      s += temp;
    }
    return s - 1;
  }

  private static void testDotProd(byte[] b1, byte[] b2, int[] results) {
    expectEquals(results[0], testDotProdSimple(b1, b2));
    expectEquals(results[1], testDotProdComplex(b1, b2));
    expectEquals(results[2], testDotProdSimpleUnsigned(b1, b2));
    expectEquals(results[3], testDotProdComplexUnsigned(b1, b2));
    expectEquals(results[4], testDotProdComplexUnsignedCastedToSigned(b1, b2));
    expectEquals(results[5], testDotProdComplexSignedCastedToUnsigned(b1, b2));
    expectEquals(results[6], testDotProdSignedWidening(b1, b2));
    expectEquals(results[7], testDotProdParamSigned(b1, b2, -128));
    expectEquals(results[8], testDotProdParamUnsigned(b1, b2, -128));
    expectEquals(results[9], testDotProdIntParam(b1, b2, -128));
    expectEquals(results[10], testDotProdSignedToChar(b1, b2));
    expectEquals(results[11], testDotProdSimpleCastedToSignedByte(b1, b2));
    expectEquals(results[12], testDotProdSimpleCastedToUnsignedByte(b1, b2));
    expectEquals(results[13], testDotProdSimpleUnsignedCastedToSignedByte(b1, b2));
    expectEquals(results[14], testDotProdSimpleUnsignedCastedToUnsignedByte(b1, b2));
    expectEquals(results[15], testDotProdSimpleCastedToShort(b1, b2));
    expectEquals(results[16], testDotProdSimpleCastedToChar(b1, b2));
    expectEquals(results[17], testDotProdSimpleUnsignedCastedToShort(b1, b2));
    expectEquals(results[18], testDotProdSimpleUnsignedCastedToChar(b1, b2));
    expectEquals(results[19], testDotProdSimpleUnsignedCastedToLong(b1, b2));
    expectEquals(results[20], testDotProdUnsignedSigned(b1, b2));
  }

  public static void testByteCases() {
    byte[] b1_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    byte[] b2_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    int[] results_1 = { 64516, 65548, 64516, 65548, 65548, 65548, 64516, -65024, 65024, -65024,
                        64516, 4, 4, 4, 4, 64516, 64516, 64516, 64516, 64516, 64516 };
    testDotProd(b1_1, b2_1, results_1);

    byte[] b1_2 = { 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    byte[] b2_2 = { 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    int[] results_2 = { 80645, 81931, 80645, 81931, 81931, 81931, 80645, -81280, 81280, -81280,
                        80645, 5, 5, 5, 5, 80645, 80645, 80645, 80645, 80645, 80645 };
    testDotProd(b1_2, b2_2, results_2);

    byte[] b1_3 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    byte[] b2_3 = {  127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  127,  127,  127,  127 };
    int[] results_3 = { -81280, 81291, 81280, 82571, 81291, 82571, -81280, -81280, 81280, -81280,
                        41534080, -640, 640, -640, 640, -81280, 246400, 81280, 81280, 81280, 81280 };
    testDotProd(b1_3, b2_3, results_3);

    byte[] b1_4 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    byte[] b2_4 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    int[] results_4 = { 81920, 80656, 81920, 83216, 80656, 83216, 81920, 81920, 81920, 81920,
                       -83804160, 0, 0, 0, 0, 81920, 81920, 81920, 81920, 81920, -81920 };
    testDotProd(b1_4, b2_4, results_4);
  }

  /// CHECK-START: int Main.testDotProdSimple(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdSimple(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Int16  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimple(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplex(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC1:i\d+>>   Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:s\d+>>  TypeConversion [<<AddC1>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC2:i\d+>>   Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:s\d+>>  TypeConversion [<<AddC2>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplex(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int16  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplex(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((short)(a[i] + 1)) * ((short)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsigned(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdSimpleUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimpleUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplexUnsigned(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:c\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:c\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplexUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i] + 1)) * ((char)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplexUnsignedCastedToSigned(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:s\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:s\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplexUnsignedCastedToSigned(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int16  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsignedCastedToSigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((short)(a[i] + 1)) * ((short)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdComplexSignedCastedToUnsigned(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:c\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:c\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int Main.testDotProdComplexSignedCastedToUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexSignedCastedToUnsigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i] + 1)) * ((char)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdSignedToInt(short[], short[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int16
  public static final int testDotProdSignedToInt(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((int)(a[i])) * ((int)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdParamSigned(short[], short[], int) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int16
  public static final int testDotProdParamSigned(short[] a, short[] b, int x) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (short)(x) * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdParamUnsigned(char[], char[], int) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Uint16
  public static final int testDotProdParamUnsigned(char[] a, char[] b, int x) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (char)(x) * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdIntParam(short[], short[], int) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdIntParam(short[] a, short[] b, int x) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = b[i] * (x);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int Main.testDotProdSignedToChar(short[], short[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Uint16
  public static final int testDotProdSignedToChar(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i])) * ((char)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  // Cases when result of Mul is type-converted are not supported.

  /// CHECK-START: int Main.testDotProdSimpleMulCastedToSigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd type:Uint16
  public static final int testDotProdSimpleMulCastedToSigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }


  /// CHECK-START: int Main.testDotProdSimpleMulCastedToUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleMulCastedToUnsigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedMulCastedToSigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedMulCastedToSigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedMulCastedToUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedMulCastedToUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleCastedToShort(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToShort(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleCastedToChar(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToChar(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToShort(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToShort(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToChar(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToChar(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSimpleUnsignedCastedToLong(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToLong(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      long temp = (long)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  // Narrowing conversions.

  /// CHECK-START: int Main.testDotProdSignedNarrowerSigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSignedNarrowerSigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)(a[i])) * ((byte)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdSignedNarrowerUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSignedNarrowerUnsigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdUnsignedNarrowerSigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedNarrowerSigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)(a[i])) * ((byte)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdUnsignedNarrowerUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedNarrowerUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int Main.testDotProdUnsignedSigned(char[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedSigned(char[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  private static void testDotProd(short[] s1, short[] s2, char[] c1, char[] c2, int[] results) {
    expectEquals(results[0], testDotProdSimple(s1, s2));
    expectEquals(results[1], testDotProdComplex(s1, s2));
    expectEquals(results[2], testDotProdSimpleUnsigned(c1, c2));
    expectEquals(results[3], testDotProdComplexUnsigned(c1, c2));
    expectEquals(results[4], testDotProdComplexUnsignedCastedToSigned(c1, c2));
    expectEquals(results[5], testDotProdComplexSignedCastedToUnsigned(s1, s2));
    expectEquals(results[6], testDotProdSignedToInt(s1, s2));
    expectEquals(results[7], testDotProdParamSigned(s1, s2, -32768));
    expectEquals(results[8], testDotProdParamUnsigned(c1, c2, -32768));
    expectEquals(results[9], testDotProdIntParam(s1, s2, -32768));
    expectEquals(results[10], testDotProdSignedToChar(s1, s2));
    expectEquals(results[11], testDotProdSimpleMulCastedToSigned(s1, s2));
    expectEquals(results[12], testDotProdSimpleMulCastedToUnsigned(s1, s2));
    expectEquals(results[13], testDotProdSimpleUnsignedMulCastedToSigned(c1, c2));
    expectEquals(results[14], testDotProdSimpleUnsignedMulCastedToUnsigned(c1, c2));
    expectEquals(results[15], testDotProdSimpleCastedToShort(s1, s2));
    expectEquals(results[16], testDotProdSimpleCastedToChar(s1, s2));
    expectEquals(results[17], testDotProdSimpleUnsignedCastedToShort(c1, c2));
    expectEquals(results[18], testDotProdSimpleUnsignedCastedToChar(c1, c2));
    expectEquals(results[19], testDotProdSimpleUnsignedCastedToLong(c1, c2));
    expectEquals(results[20], testDotProdSignedNarrowerSigned(s1, s2));
    expectEquals(results[21], testDotProdSignedNarrowerUnsigned(s1, s2));
    expectEquals(results[22], testDotProdUnsignedNarrowerSigned(c1, c2));
    expectEquals(results[23], testDotProdUnsignedNarrowerUnsigned(c1, c2));
    expectEquals(results[24], testDotProdUnsignedSigned(c1, s2));
  }

  public static void testCharShortCases() {
    final short MAX_S = Short.MAX_VALUE;
    final short MIN_S = Short.MAX_VALUE;

    short[] s1_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    short[] s2_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    char[]  c1_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    char[]  c2_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    int[] results_1 = { 2147352578, -2147483634, 2147352578, -2147483634, -2147483634, -2147483634,
                        2147352578, -2147418112, 2147418112, -2147418112, 2147352578,
                        2, 2, 2, 2, 2, 2, 2, 2, 2147352578, 2, 130050, 2, 130050, 2147352578 };
    testDotProd(s1_1, s2_1, c1_1, c2_1, results_1);

    short[] s1_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    short[] s2_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    char[]  c1_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    char[]  c2_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    int[] results_2 = { -262140, 12, -262140, 12, 12, 12, -262140, 131072, -131072, 131072,
                        -262140, 4, 4, 4, 4, 4, 4, 4, 4, -262140, 4, 260100, 4, 260100, -262140 };
    testDotProd(s1_2, s2_2, c1_2, c2_2, results_2);

    short[] s1_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    short[] s2_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    char[]  c1_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[]  c2_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    int[] results_3 = { 2147352578, -2147483634, 2147352578, -2147483634, -2147483634,
                        -2147483634, 2147352578, -2147418112, 2147418112, -2147418112,
                        2147352578, 2, 2, 2, 2, 2, 2, 2, 2, 2147352578, 2, 130050, 2,
                        130050, 2147352578};
    testDotProd(s1_3, s2_3, c1_3, c2_3, results_3);


    short[] s1_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    short[] s2_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[]  c1_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[]  c2_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    int[] results_4 = { -1073938429, -1073741811, -1073938429, -1073741811, -1073741811,
                        -1073741811, -1073938429, 1073840128, -1073840128, 1073840128,
                        -1073938429, 3, 3, 3, 3, 3, 3, 3, 3, -1073938429, 3, 195075, 3,
                        195075, -1073938429 };
    testDotProd(s1_4, s2_4, c1_4, c2_4, results_4);
  }

  public static void main(String[] args) {
    testMixedCases();
    testByteCases();
    testCharShortCases();
    System.out.println("passed");
  }
}
