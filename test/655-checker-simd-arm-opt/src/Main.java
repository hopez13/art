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

import java.util.Arrays;

/**
 * Checker test for arm and arm64 simd optimizations.
 */
public class Main {
  static int[] arr;

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectTrue(boolean result) {
    if (!result) {
      throw new Error("Expected True");
    }
  }

  /// CHECK-START-ARM64: void Main.encodableConstants(byte[], short[], char[], int[], long[], float[], double[]) disassembly (after)
  /// CHECK-DAG: <<C1:i\d+>>   IntConstant 1
  /// CHECK-DAG: <<C2:i\d+>>   IntConstant -128
  /// CHECK-DAG: <<C3:i\d+>>   IntConstant 127
  /// CHECK-DAG: <<C4:i\d+>>   IntConstant -219
  /// CHECK-DAG: <<C5:i\d+>>   IntConstant 219
  /// CHECK-DAG: <<L6:j\d+>>   LongConstant 219
  /// CHECK-DAG: <<F7:f\d+>>   FloatConstant 2
  /// CHECK-DAG: <<F8:f\d+>>   FloatConstant 14.34
  /// CHECK-DAG: <<D9:d\d+>>   DoubleConstant 20
  /// CHECK-DAG: <<D10:d\d+>>  DoubleConstant 0
  //
  /// CHECK-IF:     hasIsaFeature("sve2") and os.environ.get('ART_FORCE_TRY_PREDICATED_SIMD') == 'true'
  //
  ///     CHECK-DAG:               VecReplicateScalar [<<C1>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<C2>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<C3>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<C4>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<C5>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<L6>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<F7>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<F8>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<D9>>,{{j\d+}}]
  ///     CHECK-DAG:               VecReplicateScalar [<<D10>>,{{j\d+}}]
  //
  /// CHECK-ELSE:
  //
  ///     CHECK-DAG:               VecReplicateScalar [<<C1>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<C2>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<C3>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<C4>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<C5>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<L6>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<F7>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<F8>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<D9>>]
  ///     CHECK-DAG:               VecReplicateScalar [<<D10>>]
  //
  /// CHECK-FI:
  private static void encodableConstants(byte[] b, short[] s, char[] c, int[] a, long[] l, float[] f, double[] d) {
    for (int i = 0; i < ARRAY_SIZE; i++) {
      b[i] += 1;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      s[i] += -128;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      c[i] += 127;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      a[i] += -219;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      a[i] += 219;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      l[i] += 219;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      f[i] += 2.0f;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      f[i] += 14.34f;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      d[i] += 20.0;
    }
    for (int i = 0; i < ARRAY_SIZE; i++) {
      d[i] += 0.0;
    }
  }

  /// CHECK-START-ARM64: void Main.SVEIntermediateAddress(int) loop_optimization (after)
  /// CHECK-DAG: VecLoad  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: VecAdd   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: VecStore loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.SVEIntermediateAddress(int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2") and os.environ.get('ART_FORCE_TRY_PREDICATED_SIMD') == 'true'
  ///     CHECK-DAG: <<IntAddr1:i\d+>> IntermediateAddress [{{l\d+}},{{i\d+}}]            loop:<<Loop:B\d+>>  outer_loop:none
  ///     CHECK-DAG:                   VecLoad [<<IntAddr1>>,{{i\d+}},{{j\d+}}]           loop:<<Loop>>       outer_loop:none
  ///     CHECK-DAG:                   VecAdd                                             loop:<<Loop>>       outer_loop:none
  ///     CHECK-DAG: <<IntAddr2:i\d+>> IntermediateAddress [{{l\d+}},{{i\d+}}]            loop:<<Loop>>       outer_loop:none
  ///     CHECK-DAG:                   VecStore [<<IntAddr2>>,{{i\d+}},{{d\d+}},{{j\d+}}] loop:<<Loop>>       outer_loop:none
  /// CHECK-FI:
  //
  /// CHECK-START-ARM64: void Main.SVEIntermediateAddress(int) GVN$after_arch (after)
  /// CHECK-IF:     hasIsaFeature("sve2") and os.environ.get('ART_FORCE_TRY_PREDICATED_SIMD') == 'true'
  ///     CHECK-DAG: <<IntAddr:i\d+>>  IntermediateAddress [{{l\d+}},{{i\d+}}]            loop:<<Loop:B\d+>>  outer_loop:none
  ///     CHECK-DAG:                   VecLoad [<<IntAddr>>,{{i\d+}},{{j\d+}}]            loop:<<Loop>>       outer_loop:none
  ///     CHECK-DAG:                   VecAdd                                             loop:<<Loop>>       outer_loop:none
  ///     CHECK-DAG:                   VecStore [<<IntAddr>>,{{i\d+}},{{d\d+}},{{j\d+}}]  loop:<<Loop>>       outer_loop:none
  /// CHECK-FI:
  static void SVEIntermediateAddress(int x) {
    for (int i = 0; i < ARRAY_SIZE; i++) {
      arr[i] += x;
    }
  }

  // Test that VecAdd is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  add z{{\d+}}.b, z{{\d+}}.b, z{{\d+}}.b
  /// CHECK-FI:
  static void checkUnpredicateVecAddInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                             loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true        loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  add z{{\d+}}.h, z{{\d+}}.h, z{{\d+}}.h
  /// CHECK-FI:
  static void checkUnpredicateVecAddUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  add z{{\d+}}.h, z{{\d+}}.h, z{{\d+}}.h
  /// CHECK-FI:
  static void checkUnpredicateVecAddInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  add z{{\d+}}.s, z{{\d+}}.s, z{{\d+}}.s
  /// CHECK-FI:
  static void checkUnpredicateVecAddInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  add z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAddInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddFloat32(float[], float[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Float32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddFloat32(float[], float[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true         loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Float32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddFloat32(float[], float[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Float32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  fadd z{{\d+}}.s, z{{\d+}}.s, z{{\d+}}.s
  /// CHECK-FI:
  static void checkUnpredicateVecAddFloat32(float[] a, float[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddFloat64(double[], double[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Float64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddFloat64(double[], double[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true         loop:none
  ///     CHECK:                VecAdd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Float64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAddFloat64(double[], double[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAdd packed_type:Float64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  fadd z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAddFloat64(double[] a, double[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] += b[i];
      }
    }
  }

  // Test that VecAnd is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndBool(boolean[], boolean[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Bool loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndBool(boolean[], boolean[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Bool loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndBool(boolean[], boolean[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAnd packed_type:Bool loop:B{{\d+}}
  ///     CHECK-NEXT:                  and z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAndBool(boolean[] a, boolean[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] &= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAnd packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  and z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAndInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] &= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                             loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true        loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAnd packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  and z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAndUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] &= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAnd packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  and z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAndInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] &= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAnd packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  and z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAndInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] &= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecAnd [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecAndInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecAnd packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  and z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecAndInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] &= b[i];
      }
    }
  }

  // Test that VecMul is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  mul z{{\d+}}.b, z{{\d+}}.b, z{{\d+}}.b
  /// CHECK-FI:
  static void checkUnpredicateVecMulInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                             loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true        loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  mul z{{\d+}}.h, z{{\d+}}.h, z{{\d+}}.h
  /// CHECK-FI:
  static void checkUnpredicateVecMulUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  mul z{{\d+}}.h, z{{\d+}}.h, z{{\d+}}.h
  /// CHECK-FI:
  static void checkUnpredicateVecMulInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  mul z{{\d+}}.s, z{{\d+}}.s, z{{\d+}}.s
  /// CHECK-FI:
  static void checkUnpredicateVecMulInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  mul z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecMulInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulFloat32(float[], float[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Float32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulFloat32(float[], float[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true         loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Float32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulFloat32(float[], float[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Float32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  fmul z{{\d+}}.s, z{{\d+}}.s, z{{\d+}}.s
  /// CHECK-FI:
  static void checkUnpredicateVecMulFloat32(float[] a, float[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulFloat64(double[], double[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Float64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulFloat64(double[], double[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true         loop:none
  ///     CHECK:                VecMul [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Float64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecMulFloat64(double[], double[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecMul packed_type:Float64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  fmul z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecMulFloat64(double[] a, double[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] *= b[i];
      }
    }
  }

  // Test that VecOr is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrBool(boolean[], boolean[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                          loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Bool loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrBool(boolean[], boolean[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]              is_nop:true      loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Bool loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrBool(boolean[], boolean[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecOr packed_type:Bool loop:B{{\d+}}
  ///     CHECK-NEXT:                  orr z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecOrBool(boolean[] a, boolean[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] |= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                          loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]              is_nop:true      loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecOr packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  orr z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecOrInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] |= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]              is_nop:true        loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecOr packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  orr z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecOrUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] |= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]              is_nop:true       loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecOr packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  orr z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecOrInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] |= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]              is_nop:true       loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecOr packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  orr z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecOrInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] |= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]              is_nop:true       loop:none
  ///     CHECK:                VecOr [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecOrInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecOr packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  orr z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecOrInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] |= b[i];
      }
    }
  }

  // Test that VecShl is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShl [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecShl [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShl packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  lsl z{{\d+}}.b, z{{\d+}}.b, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShlInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] <<= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                             loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShl [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true        loop:none
  ///     CHECK:                VecShl [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShl packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  lsl z{{\d+}}.h, z{{\d+}}.h, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShlUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] <<= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShl [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecShl [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShl packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  lsl z{{\d+}}.h, z{{\d+}}.h, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShlInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] <<= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShl [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecShl [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShl packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  lsl z{{\d+}}.s, z{{\d+}}.s, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShlInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] <<= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShl [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecShl [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShlInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShl packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  lsl z{{\d+}}.d, z{{\d+}}.d, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShlInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] <<= 3;
      }
    }
  }

  // Test that VecShr is unpredicated in predicated simd mode.
  // NOTE: Shr with unsigned type is not vectorized.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShr [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecShr [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShr packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  asr z{{\d+}}.b, z{{\d+}}.b, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShrInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] >>= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShr [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecShr [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShr packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  asr z{{\d+}}.h, z{{\d+}}.h, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShrInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] >>= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShr [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecShr [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShr packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  asr z{{\d+}}.s, z{{\d+}}.s, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShrInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] >>= 3;
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecShr [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecShr [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecShrInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecShr packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  asr z{{\d+}}.d, z{{\d+}}.d, #3
  /// CHECK-FI:
  static void checkUnpredicateVecShrInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] >>= 3;
      }
    }
  }

  // Test that VecSub is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  sub z{{\d+}}.b, z{{\d+}}.b, z{{\d+}}.b
  /// CHECK-FI:
  static void checkUnpredicateVecSubInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                             loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true        loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  sub z{{\d+}}.h, z{{\d+}}.h, z{{\d+}}.h
  /// CHECK-FI:
  static void checkUnpredicateVecSubUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  sub z{{\d+}}.h, z{{\d+}}.h, z{{\d+}}.h
  /// CHECK-FI:
  static void checkUnpredicateVecSubInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  sub z{{\d+}}.s, z{{\d+}}.s, z{{\d+}}.s
  /// CHECK-FI:
  static void checkUnpredicateVecSubInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  sub z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecSubInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubFloat32(float[], float[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Float32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubFloat32(float[], float[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true         loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Float32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubFloat32(float[], float[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Float32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  fsub z{{\d+}}.s, z{{\d+}}.s, z{{\d+}}.s
  /// CHECK-FI:
  static void checkUnpredicateVecSubFloat32(float a[], float b[], int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubFloat64(double[], double[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Float64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubFloat64(double[], double[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true         loop:none
  ///     CHECK:                VecSub [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Float64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecSubFloat64(double[], double[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecSub packed_type:Float64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  fsub z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecSubFloat64(double a[], double b[], int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] -= b[i];
      }
    }
  }

  // Test that VecUShr is unpredicated in predicated simd mode.
  // NOTE: UShr with signed type is not vectorized.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecUshrUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>> VecPredNot                                              loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:               VecUShr [{{d\d+}},{{i\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecUshrUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]                is_nop:true        loop:none
  ///     CHECK:                VecUShr [{{d\d+}},{{i\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecUshrUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecUShr packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  lsr z{{\d+}}.h, z{{\d+}}.h, #3
  /// CHECK-FI:
  static void checkUnpredicateVecUshrUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] >>>= 3;
      }
    }
  }

  // Test that VecXor is unpredicated in predicated simd mode.

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorBool(boolean[], boolean[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Bool loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorBool(boolean[], boolean[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Bool loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorBool(boolean[], boolean[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecXor packed_type:Bool loop:B{{\d+}}
  ///     CHECK-NEXT:                  eor z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecXorBool(boolean[] a, boolean[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] ^= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt8(byte[], byte[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                           loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int8 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt8(byte[], byte[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true      loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int8 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt8(byte[], byte[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecXor packed_type:Int8 loop:B{{\d+}}
  ///     CHECK-NEXT:                  eor z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecXorInt8(byte[] a, byte[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] ^= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorUint16(char[], char[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                             loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorUint16(char[], char[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true        loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Uint16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorUint16(char[], char[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecXor packed_type:Uint16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  eor z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecXorUint16(char[] a, char[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] ^= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt16(short[], short[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int16 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt16(short[], short[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int16 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt16(short[], short[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecXor packed_type:Int16 loop:B{{\d+}}
  ///     CHECK-NEXT:                  eor z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecXorInt16(short[] a, short[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] ^= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt32(int[], int[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt32(int[], int[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int32 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt32(int[], int[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecXor packed_type:Int32 loop:B{{\d+}}
  ///     CHECK-NEXT:                  eor z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecXorInt32(int[] a, int[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] ^= b[i];
      }
    }
  }

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt64(long[], long[], int) instruction_simplifier_arm64 (before)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<Pred:j\d+>>  VecPredNot                                            loop:<<Loop:B\d+>> outer_loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<Pred>>] packed_type:Int64 loop:<<Loop>>      outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt64(long[], long[], int) instruction_simplifier_arm64 (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK: <<C1:i\d+>>    IntConstant 1
  ///     CHECK: <<TrueP:j\d+>> VecPredSetAll [<<C1>>]               is_nop:true       loop:none
  ///     CHECK:                VecXor [{{d\d+}},{{d\d+}},<<TrueP>>] packed_type:Int64 loop:{{B\d+}} outer_loop:none
  /// CHECK-FI:

  /// CHECK-START-ARM64: void Main.checkUnpredicateVecXorInt64(long[], long[], int) disassembly (after)
  /// CHECK-IF:     hasIsaFeature("sve2")
  ///     CHECK:      <<C1:i\d++>> IntConstant 1
  ///     CHECK:                   VecPredSetAll [<<C1>>] is_nop:true loop:none
  ///     CHECK-NOT:                   ptrue p{{\d+}}.{{[bhsd]}}
  ///     CHECK:                   VecXor packed_type:Int64 loop:B{{\d+}}
  ///     CHECK-NEXT:                  eor z{{\d+}}.d, z{{\d+}}.d, z{{\d+}}.d
  /// CHECK-FI:
  static void checkUnpredicateVecXorInt64(long[] a, long[] b, int n) {
    for (int i = 0; i < n; i++) {
      if (a[i] != b[i]) {
        a[i] ^= b[i];
      }
    }
  }

  private static int sumArray(byte[] b, short[] s, char[] c, int[] a, long[] l, float[] f, double[] d) {
    int sum = 0;
    for (int i = 0; i < ARRAY_SIZE; i++) {
      sum += b[i] + s[i] + c[i] + a[i] + l[i] + f[i] + d[i];
    }
    return sum;
  }

  public static final int ARRAY_SIZE = 128;

  public static void checkEncodableConstants() {
    byte[] b = new byte[ARRAY_SIZE];
    short[] s = new short[ARRAY_SIZE];
    char[] c = new char[ARRAY_SIZE];
    int[] a = new int[ARRAY_SIZE];
    long[] l = new long[ARRAY_SIZE];
    float[] f = new float[ARRAY_SIZE];
    double[] d = new double[ARRAY_SIZE];

    encodableConstants(b, s, c, a, l, f, d);
    expectEquals(32640, sumArray(b, s, c, a, l, f, d));

    System.out.println("encodableConstants passed");
  }

  public static void checkSVEIntermediateAddress() {
    arr = new int[ARRAY_SIZE];

    // Setup.
    for (int i = 0; i < ARRAY_SIZE; i++) {
      arr[i] = i;
    }

    // Arithmetic operations.
    SVEIntermediateAddress(2);
    for (int i = 0; i < ARRAY_SIZE; i++) {
      expectEquals(i + 2, arr[i]);
    }

    System.out.println("SVEIntermediateAddress passed");
  }

  public static void checkUnpredicateVectorInstructions() {
    final int N = ARRAY_SIZE + 1;

    boolean[] booleanArrayA = new boolean[N];
    boolean[] booleanArrayB = new boolean[N];
    boolean[] expectedBooleanArray = new boolean[N];

    byte[] byteArrayA = new byte[N];
    byte[] byteArrayB = new byte[N];
    byte[] expectedByteArray = new byte[N];

    short[] shortArrayA = new short[N];
    short[] shortArrayB = new short[N];
    short[] expectedShortArray = new short[N];

    char[] charArrayA = new char[N];
    char[] charArrayB = new char[N];
    char[] expectedCharArray = new char[N];

    int[] intArrayA = new int[N];
    int[] intArrayB = new int[N];
    int[] expectedIntArray = new int[N];

    long[] longArrayA = new long[N];
    long[] longArrayB = new long[N];
    long[] expectedLongArray = new long[N];

    float[] floatArrayA = new float[N];
    float[] floatArrayB = new float[N];
    float[] expectedFloatArray = new float[N];

    double[] doubleArrayA = new double[N];
    double[] doubleArrayB = new double[N];
    double[] expectedDoubleArray = new double[N];

    boolean booleanA = true;
    boolean booleanB = false;
    byte byteA = 3;
    byte byteB = 2;
    char charA = 3;
    char charB = 2;
    short shortA = 3;
    short shortB = 2;
    int intA = 3;
    int intB = 2;
    long longA = 3;
    long longB = 2;
    float floatA = 3.0f;
    float floatB = 2.0f;
    double doubleA = 3.0;
    double doubleB = 2.0;

    // VecAdd

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] + byteArrayB[0]));
    checkUnpredicateVecAddInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] + charArrayB[0]));
    checkUnpredicateVecAddUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] + shortArrayB[0]));
    checkUnpredicateVecAddInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] + intArrayB[0]);
    checkUnpredicateVecAddInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] + longArrayB[0]);
    checkUnpredicateVecAddInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    Arrays.fill(floatArrayA, floatA);
    Arrays.fill(floatArrayB, floatB);
    Arrays.fill(expectedFloatArray, floatArrayA[0] + floatArrayB[0]);
    checkUnpredicateVecAddFloat32(floatArrayA, floatArrayB, N);
    expectTrue(Arrays.equals(floatArrayA, expectedFloatArray));

    Arrays.fill(doubleArrayA, doubleA);
    Arrays.fill(doubleArrayB, doubleB);
    Arrays.fill(expectedDoubleArray, doubleArrayA[0] + doubleArrayB[0]);
    checkUnpredicateVecAddFloat64(doubleArrayA, doubleArrayB, N);
    expectTrue(Arrays.equals(doubleArrayA, expectedDoubleArray));

    // VecAnd

    Arrays.fill(booleanArrayA, booleanA);
    Arrays.fill(booleanArrayB, booleanB);
    Arrays.fill(expectedBooleanArray, booleanArrayA[0] & booleanArrayB[0]);
    checkUnpredicateVecAndBool(booleanArrayA, booleanArrayB, N);
    expectTrue(Arrays.equals(booleanArrayA, expectedBooleanArray));

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] & byteArrayB[0]));
    checkUnpredicateVecAndInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] & charArrayB[0]));
    checkUnpredicateVecAndUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] & shortArrayB[0]));
    checkUnpredicateVecAndInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] & intArrayB[0]);
    checkUnpredicateVecAndInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] & longArrayB[0]);
    checkUnpredicateVecAndInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    // VecMul

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] * byteArrayB[0]));
    checkUnpredicateVecMulInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] * charArrayB[0]));
    checkUnpredicateVecMulUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] * shortArrayB[0]));
    checkUnpredicateVecMulInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] * intArrayB[0]);
    checkUnpredicateVecMulInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] * longArrayB[0]);
    checkUnpredicateVecMulInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    Arrays.fill(floatArrayA, floatA);
    Arrays.fill(floatArrayB, floatB);
    Arrays.fill(expectedFloatArray, floatArrayA[0] * floatArrayB[0]);
    checkUnpredicateVecMulFloat32(floatArrayA, floatArrayB, N);
    expectTrue(Arrays.equals(floatArrayA, expectedFloatArray));

    Arrays.fill(doubleArrayA, doubleA);
    Arrays.fill(doubleArrayB, doubleB);
    Arrays.fill(expectedDoubleArray, doubleArrayA[0] * doubleArrayB[0]);
    checkUnpredicateVecMulFloat64(doubleArrayA, doubleArrayB, N);
    expectTrue(Arrays.equals(doubleArrayA, expectedDoubleArray));

    // VecOr

    Arrays.fill(booleanArrayA, booleanA);
    Arrays.fill(booleanArrayB, booleanB);
    Arrays.fill(expectedBooleanArray, booleanArrayA[0] | booleanArrayB[0]);
    checkUnpredicateVecOrBool(booleanArrayA, booleanArrayB, N);
    expectTrue(Arrays.equals(booleanArrayA, expectedBooleanArray));

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] | byteArrayB[0]));
    checkUnpredicateVecOrInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] | charArrayB[0]));
    checkUnpredicateVecOrUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] | shortArrayB[0]));
    checkUnpredicateVecOrInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] | intArrayB[0]);
    checkUnpredicateVecOrInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] | longArrayB[0]);
    checkUnpredicateVecOrInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    // VecShl

    final int shift = 3;

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] << shift));
    checkUnpredicateVecShlInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] << shift));
    checkUnpredicateVecShlUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] << shift));
    checkUnpredicateVecShlInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] << shift);
    checkUnpredicateVecShlInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] << shift);
    checkUnpredicateVecShlInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    // VecShr

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] >> shift));
    checkUnpredicateVecShrInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] >> shift));
    checkUnpredicateVecShrInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] >> shift);
    checkUnpredicateVecShrInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] >> shift);
    checkUnpredicateVecShrInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    // VecSub

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] - byteArrayB[0]));
    checkUnpredicateVecSubInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] - charArrayB[0]));
    checkUnpredicateVecSubUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] - shortArrayB[0]));
    checkUnpredicateVecSubInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] - intArrayB[0]);
    checkUnpredicateVecSubInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] - longArrayB[0]);
    checkUnpredicateVecSubInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));

    Arrays.fill(floatArrayA, floatA);
    Arrays.fill(floatArrayB, floatB);
    Arrays.fill(expectedFloatArray, floatArrayA[0] - floatArrayB[0]);
    checkUnpredicateVecSubFloat32(floatArrayA, floatArrayB, N);
    expectTrue(Arrays.equals(floatArrayA, expectedFloatArray));

    Arrays.fill(doubleArrayA, doubleA);
    Arrays.fill(doubleArrayB, doubleB);
    Arrays.fill(expectedDoubleArray, doubleArrayA[0] - doubleArrayB[0]);
    checkUnpredicateVecSubFloat64(doubleArrayA, doubleArrayB, N);
    expectTrue(Arrays.equals(doubleArrayA, expectedDoubleArray));

    // VecUshr

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] >>> shift));
    checkUnpredicateVecUshrUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    // VecXor

    Arrays.fill(booleanArrayA, booleanA);
    Arrays.fill(booleanArrayB, booleanB);
    Arrays.fill(expectedBooleanArray, booleanArrayA[0] ^ booleanArrayB[0]);
    checkUnpredicateVecXorBool(booleanArrayA, booleanArrayB, N);
    expectTrue(Arrays.equals(booleanArrayA, expectedBooleanArray));

    Arrays.fill(byteArrayA, byteA);
    Arrays.fill(byteArrayB, byteB);
    Arrays.fill(expectedByteArray, (byte) (byteArrayA[0] ^ byteArrayB[0]));
    checkUnpredicateVecXorInt8(byteArrayA, byteArrayB, N);
    expectTrue(Arrays.equals(byteArrayA, expectedByteArray));

    Arrays.fill(charArrayA, charA);
    Arrays.fill(charArrayB, charB);
    Arrays.fill(expectedCharArray, (char) (charArrayA[0] ^ charArrayB[0]));
    checkUnpredicateVecXorUint16(charArrayA, charArrayB, N);
    expectTrue(Arrays.equals(charArrayA, expectedCharArray));

    Arrays.fill(shortArrayA, shortA);
    Arrays.fill(shortArrayB, shortB);
    Arrays.fill(expectedShortArray, (short) (shortArrayA[0] ^ shortArrayB[0]));
    checkUnpredicateVecXorInt16(shortArrayA, shortArrayB, N);
    expectTrue(Arrays.equals(shortArrayA, expectedShortArray));

    Arrays.fill(intArrayA, intA);
    Arrays.fill(intArrayB, intB);
    Arrays.fill(expectedIntArray, intArrayA[0] ^ intArrayB[0]);
    checkUnpredicateVecXorInt32(intArrayA, intArrayB, N);
    expectTrue(Arrays.equals(intArrayA, expectedIntArray));

    Arrays.fill(longArrayA, longA);
    Arrays.fill(longArrayB, longB);
    Arrays.fill(expectedLongArray, longArrayA[0] ^ longArrayB[0]);
    checkUnpredicateVecXorInt64(longArrayA, longArrayB, N);
    expectTrue(Arrays.equals(longArrayA, expectedLongArray));
  };

  public static void main(String[] args) {
    checkEncodableConstants();
    checkSVEIntermediateAddress();
    checkUnpredicateVectorInstructions();
  }
}
