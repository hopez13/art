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

/**
 * Tests for zero vectorization.
 */
public class Main {

  /// CHECK-START: void Main.staticallyAligned4(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                      loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<Zero>>,<<AddI:i\d+>>]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Phi>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Phi>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyAligned4(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                      loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Four:i\d+>> IntConstant 4                      loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<One>>]       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<Zero>>,<<AddI:i\d+>>]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>> VecLoad [<<Par>>,<<Phi>>]          alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecAdd [<<Load>>,<<Repl>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [<<Par>>,<<Phi>>,<<Add>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<Four>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:               ArrayGet
  /// CHECK-NOT:               ArraySet
  static void staticallyAligned4(int[] a) {
    for (int i = 0; i < 4; i++) {  // just one vector iteration, no cleanup
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyAligned8(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                      loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<Zero>>,<<AddI:i\d+>>]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Phi>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Phi>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyAligned8(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Four:i\d+>>  IntConstant 4                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi:i\d+>>   Phi [<<Zero>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>> VecLoad [<<Par>>,<<Phi>>]            alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add1:d\d+>>  VecAdd [<<Load1>>,<<Repl>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Phi>>,<<Add1>>]  alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Incr:i\d+>>  Add [<<Phi>>,<<Four>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>> VecLoad [<<Par>>,<<Incr>>]           alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Load2>>,<<Repl>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Incr>>,<<Add2>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Incr>>,<<Four>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                ArrayGet
  /// CHECK-NOT:                ArraySet
  static void staticallyAligned8(int[] a) {
    for (int i = 0; i < 8; i++) {  // allows for SIMD unrolling, no cleanup
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyAlignedN(int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                      loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<Zero>>,<<AddI:i\d+>>]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Phi>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Phi>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyAlignedN(int[], int) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                        loop:none
  /// CHECK-DAG: <<Four:i\d+>> IntConstant 4                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>> Phi [<<Zero>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>>  outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>> VecLoad [<<Par>>,<<Phi1>>]           alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add1:d\d+>> VecAdd [<<Load>>,<<Repl>>]           loop:<<Loop>>       outer_loop:none
  /// CHECK-DAG:               VecStore [<<Par>>,<<Phi1>>,<<Add1>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi1>>,<<Four>>]              loop:<<Loop>>       outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>> Phi [<<Phi1>>,<<AddC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Phi2>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Add2:i\d+>> Add [<<Get>>,<<One>>]                loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Phi2>>,<<Add2>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC>>      Add [<<Phi2>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyAlignedN(int[] a, int n) {
    for (int i = 0; i < n; i++) {  // needs cleanup
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyMisAligned8(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<One>>,<<AddI:i\d+>>]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Phi>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Phi>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyMisAligned8(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Four:i\d+>>  IntConstant 4                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>  Phi [<<Zero>>,<<AddP:i\d+>>]         loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj1:i\d+>>  Add [<<Phi1>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Adj1>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj1>>,<<Add1>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP>>       Add [<<Phi1>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>  Phi [<<Phi1>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj2:i\d+>>  Add [<<Phi2>>,<<One>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Adj2>>]           alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Load>>,<<Repl>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Adj2>>,<<Add2>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi2>>,<<Four>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                ArrayGet
  /// CHECK-NOT:                ArraySet
  static void staticallyMisAligned8(int[] a) {
    for (int i = 1; i < 8; i++) {  // needs to statically peel, no cleanup
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyMisAlignedN(int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<One>>,<<AddI:i\d+>>]        loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Phi>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Phi>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyMisAlignedN(int[], int) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Four:i\d+>>  IntConstant 4                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>  Phi [<<Zero>>,<<AddP:i\d+>>]         loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj1:i\d+>>  Add [<<Phi1>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Adj1>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj1>>,<<Add1>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP>>       Add [<<Phi1>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>  Phi [<<Phi1>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj2:i\d+>>  Add [<<Phi2>>,<<One>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Adj2>>]           alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Load>>,<<Repl>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Adj2>>,<<Add2>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi2>>,<<Four>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>  Phi [<<Phi2>>,<<AddC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj3:i\d+>>  Add [<<Phi3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Get3:i\d+>>  ArrayGet [<<Par>>,<<Adj3>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Add3:i\d+>>  Add [<<Get3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj3>>,<<Add3>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC>>       Add [<<Phi3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyMisAlignedN(int[] a, int n) {
    for (int i = 1; i < n; i++) {  // needs to statically peel and cleanup
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyUnknownAligned8(int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Off:i\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                      loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<Zero>>,<<AddI:i\d+>>]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj:i\d+>>  Add [<<Off>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Adj>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Adj>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyUnknownAligned8(int[], int) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Off:i\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Four:i\d+>>  IntConstant 4                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>  Phi [<<Zero>>,<<AddP:i\d+>>]         loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj1:i\d+>>  Add [<<Phi1>>,<<Off>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Adj1>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj1>>,<<Add1>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP>>       Add [<<Phi1>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>  Phi [<<Phi1>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj2:i\d+>>  Add [<<Phi2>>,<<Off>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Adj2>>]           alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Load>>,<<Repl>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Adj2>>,<<Add2>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi2>>,<<Four>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>  Phi [<<Phi2>>,<<AddC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj3:i\d+>>  Add [<<Phi3>>,<<Off>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Get3:i\d+>>  ArrayGet [<<Par>>,<<Adj3>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Add3:i\d+>>  Add [<<Get3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj3>>,<<Add3>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC>>       Add [<<Phi3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyUnknownAligned8(int[] a, int off) {
    for (int i = 0; i < 8; i++) {  // needs to dynamically peel and cleanup
      a[off + i] += 1;
    }
  }

  /// CHECK-START: void Main.staticallyUnknownAlignedN(int[], int, int) loop_optimization (before)
  /// CHECK-DAG: <<Par:l\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Off:i\d+>>  ParameterValue                     loop:none
  /// CHECK-DAG: <<Zero:i\d+>> IntConstant 0                      loop:none
  /// CHECK-DAG: <<One:i\d+>>  IntConstant 1                      loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi [<<Zero>>,<<AddI:i\d+>>]       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj:i\d+>>  Add [<<Off>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>  ArrayGet [<<Par>>,<<Adj>>]         loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [<<Par>>,<<Adj>>,<<Add>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI>>      Add [<<Phi>>,<<One>>]              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-ARM64: void Main.staticallyUnknownAlignedN(int[], int, int) loop_optimization (after)
  /// CHECK-DAG: <<Par:l\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Off:i\d+>>   ParameterValue                       loop:none
  /// CHECK-DAG: <<Zero:i\d+>>  IntConstant 0                        loop:none
  /// CHECK-DAG: <<One:i\d+>>   IntConstant 1                        loop:none
  /// CHECK-DAG: <<Four:i\d+>>  IntConstant 4                        loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>  Phi [<<Zero>>,<<AddP:i\d+>>]         loop:<<Peel:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj1:i\d+>>  Add [<<Phi1>>,<<Off>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Get:i\d+>>   ArrayGet [<<Par>>,<<Adj1>>]          loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Add1:i\d+>>  Add [<<Get>>,<<One>>]                loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj1>>,<<Add1>>] loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<AddP>>       Add [<<Phi1>>,<<One>>]               loop:<<Peel>>      outer_loop:none
  /// CHECK-DAG: <<Repl:d\d+>>  VecReplicateScalar [<<One>>]         loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>  Phi [<<Phi1>>,<<AddI:i\d+>>]         loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj2:i\d+>>  Add [<<Phi2>>,<<Off>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load:d\d+>>  VecLoad [<<Par>>,<<Adj2>>]           alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Add2:d\d+>>  VecAdd [<<Load>>,<<Repl>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                VecStore [<<Par>>,<<Adj2>>,<<Add2>>] alignment:ALIGN(16,0) loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<AddI>>       Add [<<Phi2>>,<<Four>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Phi3:i\d+>>  Phi [<<Phi2>>,<<AddC:i\d+>>]         loop:<<Clean:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Adj3:i\d+>>  Add [<<Phi3>>,<<Off>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Get3:i\d+>>  ArrayGet [<<Par>>,<<Adj3>>]          loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<Add3:i\d+>>  Add [<<Get3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG:                ArraySet [<<Par>>,<<Adj3>>,<<Add3>>] loop:<<Clean>>      outer_loop:none
  /// CHECK-DAG: <<AddC>>       Add [<<Phi3>>,<<One>>]               loop:<<Clean>>      outer_loop:none
  static void staticallyUnknownAlignedN(int[] a, int off, int n) {
    for (int i = 0; i < n; i++) {  // needs to dynamically peel and cleanup
      a[off + i] += 1;
    }
  }

  //
  // Test drivers.
  //

  private static void test1() {
    int[] a = new int[10];
    staticallyAligned4(a);
    for (int i = 0; i < a.length; i++) {
      int e = i < 4 ? 1 : 0;
      expectEquals(e, a[i]);
    }
  }

  private static void test2() {
    int[] a = new int[10];
    staticallyAligned8(a);
    for (int i = 0; i < a.length; i++) {
      int e = i < 8 ? 1 : 0;
      expectEquals(e, a[i]);
    }
  }

  private static void test3() {
    for (int n = 0; n <= 10; n++) {
      int[] a = new int[10];
      staticallyAlignedN(a, n);
      for (int i = 0; i < a.length; i++) {
        int e = i < n ? 1 : 0;
        expectEquals(e, a[i]);
      }
    }
  }

  private static void test4() {
    int[] a = new int[10];
    staticallyMisAligned8(a);
    for (int i = 0; i < a.length; i++) {
      int e = (0 < i && i < 8) ? 1 : 0;
      expectEquals(e, a[i]);
    }
  }

  private static void test5() {
    for (int n = 0; n <= 10; n++) {
      int[] a = new int[10];
      staticallyMisAlignedN(a, n);
      for (int i = 0; i < a.length; i++) {
        int e = (0 < i && i < n) ? 1 : 0;
        expectEquals(e, a[i]);
      }
    }
  }

  private static void test6() {
    for (int off = 0; off <= 8; off++) {
      int[] a = new int[16];
      staticallyUnknownAligned8(a, off);
      for (int i = 0; i < a.length; i++) {
        int e = (off <= i && i < off + 8) ? 1 : 0;
        expectEquals(e, a[i]);
      }
    }
  }

  private static void test7() {
    for (int off = 0; off <= 8; off++) {
      for (int n = 0; n <= 8; n++) {
        int[] a = new int[16];
        staticallyUnknownAlignedN(a, off, n);
        for (int i = 0; i < a.length; i++) {
          int e = (off <= i && i < off + n) ? 1 : 0;
          expectEquals(e, a[i]);
        }
      }
    }
  }

  public static void main(String[] args) {
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
