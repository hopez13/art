/*
 * Copyright (C) 2017 The Android Open Source Project
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

//
// Test loop optimizations, in particular scalar loop peeling and unrolling.
public class Main {

  static final int LENGTH = 4 * 1024;
  int[] a = new int[LENGTH];

  private static final int LENGTH_A = LENGTH;
  private static final int LENGTH_B = 16;
  private static final int RESULT_POS = 4;

  double[][] mA;
  double[][] mB;
  double[][] mC;

  public Main() {
    mA = new double[LENGTH_A][];
    mB = new double[LENGTH_B][];
    mC = new double[LENGTH_B][];
    for (int i = 0; i < LENGTH_A; i++) {
      mA[i] = new double[LENGTH_B];
    }

    for (int i = 0; i < LENGTH_B; i++) {
      mB[i] = new double[LENGTH_A];
      mC[i] = new double[LENGTH_B];
    }
  }

  private static final void initMatrix(double[][] m) {
    for (int i = 0; i < m.length; i++) {
      for (int j = 0; j < m[i].length; j++) {
        m[i][j] = (double) (i / (j + 1));
      }
    }
  }

  private static final void initIntArray(int[] a) {
    for (int i = 0; i < LENGTH; i++) {
      a[i] = i % 4;
    }
  }

  /// CHECK-START: void Main.unrollingLoadStoreElimination(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>   IntConstant 4094                          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>     Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>   GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>      If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0:i\d+>>    ArrayGet [<<Array>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>  Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>     Add [<<Get0>>,<<Get1>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<Phi>>,<<Add>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                  ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.unrollingLoadStoreElimination(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>   IntConstant 4094                          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>     Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>   GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>      If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0:i\d+>>    ArrayGet [<<Array>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>  Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>     Add [<<Get0>>,<<Get1>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<Phi>>,<<Add>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>  GreaterThanOrEqual [<<IndAdd>>,<<Limit>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>     If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0A:i\d+>>   ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAddA:i\d+>> Add [<<IndAdd>>,<<Const1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1A:i\d+>>   ArrayGet [<<Array>>,<<IndAddA>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddA:i\d+>>    Add [<<Get0A>>,<<Get1A>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<IndAdd>>,<<AddA>>]  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                  ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingLoadStoreElimination(int[] a) {
    for (int i = 0; i < LENGTH - 2; i++) {
      a[i] += a[i + 1];
    }
  }

  /// CHECK-START: void Main.unrollingTwoLatches(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
  /// CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}},{{i\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}},{{i\d+}}]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.unrollingTwoLatches(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
  /// CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}},{{i\d+}}]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}},{{i\d+}}]      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddS1:i\d+>>    Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiS>>]     loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<PhiSA:i\d+>>    Phi [<<AddS1>>,<<PhiS>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddIA:i\d+>>    Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<RemA:i\d+>>     Rem [<<AddIA>>,<<Const2>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<NEA:z\d+>>      NotEqual [<<RemA>>,<<Const0>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   If [<<NEA>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddS1A:i\d+>>   Add [<<PhiSA>>,<<Const1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiSA>>]    loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingTwoLatches(int[] a) {
    int i = 0;
    int s = 128;
    while (i++ < LENGTH - 2) {
      if (i % 2 == 0) {
        a[i] = s++;
      }
    }
  }

  // Simple check that loop unrolling has happened.
  //
  /// CHECK-START: void Main.unrollingSwitch(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4096                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.unrollingSwitch(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4096                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingSwitch(int[] a) {
    for (int i = 0; i < LENGTH; i++) {
      switch (i % 3) {
        case 2:
          a[i]++;
          break;
        default:
          break;
      }
    }
  }

  // Simple check that loop unrolling has happened.
  //
  /// CHECK-START: void Main.unrollingSwapElements(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.unrollingSwapElements(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingSwapElements(int[] array) {
    for (int i = 0; i < LENGTH - 2; i++) {
      if (array[i] > array[i + 1]) {
        int temp = array[i + 1];
        array[i + 1] = array[i];
        array[i] = temp;
      }
    }
  }

  // Simple check that loop unrolling has happened.
  //
  /// CHECK-START: void Main.unrollingRInnerproduct(double[][], double[][], double[][], int, int) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4096                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START-ARM64: void Main.unrollingRInnerproduct(double[][], double[][], double[][], int, int) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4096                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingRInnerproduct(double[][] result,
                                                   double[][] a,
                                                   double[][] b,
                                                   int row,
                                                   int column) {
    // computes the inner product of A[row,*] and B[*,column]
    int i;
    result[row][column] = 0.0f;
    for (i = 0; i < LENGTH_A; i++) {
      result[row][column] = result[row][column] + a[row][i] * b[i][column];
    }
  }

  //------------------------------------------------------------------------------------------------
  // The following section contains compiler test for kForceLoopUnrolling.
  //------------------------------------------------------------------------------------------------

  // relatively simple loop
  void sbc11(int[] a, int x) {
    int[] b = new int[128];
    for (int i = 0; i < 128; i++) {
      a[x]++;
      b[i] = b[i] + 1;
    }
  }

  // nested loop
  // [[[]]]
  void sbc21(int[] a, int[] b, int x) {
    for (int k = 0; k < 128; k++) {
      for (int j = 0; j < 128; j++) {
        for (int i = 0; i < 128; i++) {
          b[x]++;
          a[i] = a[i] + 1;
        }
      }
    }
  }

  // nested loop:
  // [
  //   if [] else []
  // ]
  void sbc22(int[] a, int[] b, int x) {
    for (int k = 0; k < 128; k++) {
      if (x > 100) {
        for (int j = 0; j < 128; j++) {
          a[x] ++;
          b[j] = b[j] + 1;
        }
      } else {
        for (int i = 0; i < 128; i++) {
          b[x]++;
          a[i] = a[i] + 1;
        }
      }
    }
  }

  // nested loop:
  // [
  //   [] [] [] []
  // ]
  void sbc23(int[] a, int[] b, int[] c, int[] d, int x) {
    for (int t = 0; t < 128; t++) {
      for (int i = 0; i < 128; i++) {
        b[x]++;
        a[i] = a[i] + 1;
      }
      for (int j = 0; j < 128; j++) {
        a[x] ++;
        b[j] = b[j] + 1;
      }
      for (int k = 0; k < 128; k++) {
        d[x] ++;
        c[k] = c[k] + 1;
      }
      for (int l = 0; l < 128; l++) {
        c[x] ++;
        b[l] = b[l] + 1;
      }
    }
  }

  // complicated control flow
  void sbc31(boolean[] a, int x) {
    int[] b = new int[128];
    for (int i = 0; i < 128; i++) {
      if (a[0]) {
        if (a[1]) {
          if (a[2]) {
            if (a[3]) {
              if (a[4]) {
                if (a[5]) {
                  if (a[6]) {
                    if (a[7]) {
                      b[i] = b[i] + 1;
                    }
                  }
                }
              }
            }
          }
        }
      } else {
        b[i] = b[i] - 10;
      }
    }
  }

  // complicated switch case control flow, without PackedSwitch
  void sbc41(int[] b, int x) {
    for (int k = 0; k < 128; k++) {
      for (int j = 0; j < 128; j++) {
        for (int i = 0; i < 128; i++) {
          switch (x) {
            case 1:
              b[i] = b[i] + 10;
              break;
            case 2:
              b[i] = b[i] + 20;
              break;
            case 3:
              b[i] = b[i] + 20;
              break;
            default:
              b[x] = 10;
              break;
          }
        }
      }
    }
  }

  // switch-case with PackedSwitch generated
  void sbc42(int[] b, int x) {
    for (int i = 0; i < 128; i++) {
      switch (x) {
        case 1:
          b[i] = b[i] + 10;
          break;
        case 2:
          b[i] = b[i] + 20;
          break;
        case 3:
          b[i] = b[i] + 30;
          break;
        case 4:
          b[i] = b[i] + 40;
          break;
        case 5:
          b[i] = b[i] + 50;
          break;
        case 6:
          b[i] = b[i] + 60;
          break;
        case 7:
          b[i] = b[i] + 70;
          break;
        default:
          b[i]++;
          break;
      }
    }
  }

  // complicated switch case control flow,
  // with complicated control flow in some case block;
  // without PackedSwitch.
  void sbc43(boolean[] a, int[] b, int x) {
    for (int k = 0; k < 128; k++) {
      for (int j = 0; j < 128; j++) {
        for (int i = 0; i < 128; i++) {
          switch (x) {
            case 1:
              if (a[0]) {
                if (a[1]) {
                  if (a[2]) {
                    b[i]++;
                  }
                }
              }
              break;
            case 2:
              if (!a[0]) {
                if (!a[1]) {
                  if (!a[2]) {
                    b[i]--;
                  }
                }
              }
              break;
            case 3:
              b[i] = b[i] + 20;
              break;
            default:
              b[x] = 10;
              break;
          }
        }
      }
    }
  }

  private static void cf_example(int[] a, int[] b) {
    for (int i = 3; i < 128; i++) {
      int s = a[i] += b[i];
      if (i % 4 == 1) {
        s += 2;
        continue;
      }
      if (a[i] == b[i] / 2) {
        b[i] = a[i] + s;
      }
      a[i]++;
    }
  }

  private static void invariant1(int[] a) {
    boolean flag = true;
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (!flag) {
        a[i] += 1;
      }
      flag = false;
    }
  }

  private static void invariant2(int[] a, int n, int m) {
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (m < n && n < 0) {
        a[i] += 1;
      }
    }
  }

  private static void invariant3(int[] a, int n, int m) {
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (m < n) {
        break;
      }
    }
  }

  private static void invariant3_return(int[] a, int n, int m) {
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (m < n) {
        return;
      }
    }
  }

  private static void invariant3_nested(int[] a, int[] b, int n, int m) {
    for (int j = 1; j < 5; j++) {
      for (int i = 1; i < 128; i++) {
        a[i] += 4;
        if (m < n) {
          break;
        }
      }

      for (int i = 1; i < 128; i++) {
        a[i] -= 4;
        if (m < n) {
          break;
        }
      }
    }
  }

  public static boolean foo(int a) {
    return Math.min(a, 4) << a < 4;
  }

  private static short break_outer(int[] a) {
    outer:
    for (int i = 1; i < 128; i++) {
      for (int j = 0; j < 128; j++) {
        if (foo(i + j)) {
          break outer;
        }
        a[j] = i;
      }
      a[i] = i;
    }
    return 0;
  }
  //------------------------------------------------------------------------------------------------

  public boolean verifyUnrolling() {
    initIntArray(a);
    int expected = 2363604;
    int found = 0;
    unrollingSwitch(a);
    unrollingLoadStoreElimination(a);
    unrollingSwapElements(a);
    unrollingTwoLatches(a);

    for (int i = 0; i < LENGTH; i++) {
      found += a[i];
    }
    found += (int)mC[RESULT_POS][RESULT_POS];

    if (found != expected) {
      System.out.println("ERROR: Expected " + expected + " but found " + found);
      return false;
    }

    return true;
  }

  public static void main(String[] args) {
    Main obj = new Main();
    if (obj.verifyUnrolling()) {
      System.out.println("passed");
    }
  }
}
