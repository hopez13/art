/*
 * Copyright (C) 2016 The Android Open Source Project
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

// Regression test case where the register allocation resolvers used
// to insert a ParallelMove instruction after an InstanceFieldGet
// instruction following a LoadReadBarrierState instruction. This was
// caught by the GraphChecker on ARM, ARM64 and x86-64.

public class ClassWithManyFields {
  Object o00; Object o01; Object o02; Object o03; Object o04; Object o05; Object o06;
  Object o07; Object o08; Object o09; Object o10; Object o11; Object o12; Object o13;
  Object o14; Object o15; Object o16; Object o17; Object o18; Object o19; Object o20;
  Object o21; Object o22; Object o23; Object o24; Object o25; Object o26; Object o27;
  Object o28; Object o29; Object o30; Object o31; Object o32; Object o33; Object o34;
  Object o35; Object o36; Object o37; Object o38; Object o39; Object o40; Object o41;
  Object o42; Object o43; Object o44; Object o45; Object o46; Object o47; Object o48;
  Object o49; Object o50;

  ClassWithCtorWithManyArgs aa;

  boolean b;

  Object m() { return null; }

  // On ARM and x86-64, there are two ParallelMove instructions
  // inserted by the register allocation resolver before two
  // InstanceFieldGet instructions enclosed between a
  // LoadReadBarrierState instruction and a
  // MarkReferences{Explicit,Implicit}RBState instruction, to save the
  // output of a NotEqual condition, used at the end of the method by
  // an InstanceFieldSet instruction.

  /// CHECK-START-ARM-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): void ClassWithManyFields.a(java.lang.Object) register (after)
  /// CHECK:      <<This:l\d+>>    ParameterValue
  /// CHECK:      <<Arg:l\d+>>     ParameterValue
  /// CHECK:      <<Null:l\d+>>    NullConstant
  /// CHECK:      <<Cond:z\d+>>    NotEqual [<<Arg>>,<<Null>>] locations:[r2,#null]->r5
  //
  // Note: We explicitly specify the liveness of the following
  // instructions to help Checker find them, as there are multiple
  // similar instances of LoadReadBarrierState and ParallelMove.
  //
  /// CHECK:      <<RBState:i\d+>> LoadReadBarrierState [<<This>>] liveness:64 locations:[r6]->r1
  /// CHECK-NEXT:                  ParallelMove liveness:66 moves:[r5->r7] liveness:66
  /// CHECK-NEXT: <<Field1:l\d+>>  InstanceFieldGet [<<This>>] field_name:ClassWithManyFields.o06 field_type:PrimNot generates_own_read_barrier:false liveness:66 locations:[r6]->r5
  /// CHECK-NEXT:                  ParallelMove liveness:68 moves:[r7->r8] liveness:68
  /// CHECK-NEXT: <<Field2:l\d+>>  InstanceFieldGet [<<This>>] field_name:ClassWithManyFields.o07 field_type:PrimNot generates_own_read_barrier:false liveness:68 locations:[r6]->r7
  /// CHECK-NEXT:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>] liveness:70 locations:[r1,r5,r7]->invalid
  //
  /// CHECK:                       InstanceFieldSet [<<This>>,<<Cond>>] field_name:ClassWithManyFields.b field_type:PrimBoolean locations:[r6,r8]->invalid

  /// CHECK-START-ARM64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): void ClassWithManyFields.a(java.lang.Object) register (after)
  /// CHECK:      <<This:l\d+>>    ParameterValue
  /// CHECK:      <<Arg:l\d+>>     ParameterValue
  /// CHECK:      <<Null:l\d+>>    NullConstant
  /// CHECK:      <<Cond:z\d+>>    NotEqual [<<Arg>>,<<Null>>] locations:[x2,#null]->x20
  //
  // Note: We explicitly specify the liveness of the following
  // instructions to help Checker find them, as there are multiple
  // similar instances of LoadReadBarrierState and ParallelMove.
  //
  /// CHECK:      <<RBState:i\d+>> LoadReadBarrierState [<<This>>] liveness:126 locations:[x21]->x1
  /// CHECK-NEXT:                  ParallelMove liveness:128 moves:[x20->x22] liveness:128
  /// CHECK-NEXT: <<Field1:l\d+>>  InstanceFieldGet [<<This>>] field_name:ClassWithManyFields.o22 field_type:PrimNot generates_own_read_barrier:false liveness:128 locations:[x21]->x20
  /// CHECK-NEXT:                  ParallelMove liveness:130 moves:[x22->x23] liveness:130
  /// CHECK-NEXT: <<Field2:l\d+>>  InstanceFieldGet [<<This>>] field_name:ClassWithManyFields.o23 field_type:PrimNot generates_own_read_barrier:false liveness:130 locations:[x21]->x22
  /// CHECK-NEXT:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>] liveness:132 locations:[x1,x20,x22]->invalid
  //
  /// CHECK:                       InstanceFieldSet [<<This>>,<<Cond>>] field_name:ClassWithManyFields.b field_type:PrimBoolean locations:[x21,x23]->invalid

  /// CHECK-START-X86_64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): void ClassWithManyFields.a(java.lang.Object) register (after)
  /// CHECK:      <<This:l\d+>>    ParameterValue
  /// CHECK:      <<Arg:l\d+>>     ParameterValue
  /// CHECK:      <<Null:l\d+>>    NullConstant
  /// CHECK:      <<Cond:z\d+>>    NotEqual [<<Arg>>,<<Null>>] locations:[rdx,#null]->rbx
  //
  // Note: We explicitly specify the liveness of the following
  // instructions to help Checker find them, as there are multiple
  // similar instances of LoadReadBarrierState and ParallelMove.
  //
  /// CHECK:                       LoadReadBarrierState [<<This>>] liveness:78 locations:[rbp]->invalid
  /// CHECK-NEXT:                  ParallelMove liveness:80 moves:[rbx->r12] liveness:80
  /// CHECK-NEXT: <<Field1:l\d+>>  InstanceFieldGet [<<This>>] field_name:ClassWithManyFields.o10 field_type:PrimNot generates_own_read_barrier:false liveness:80 locations:[rbp]->rbx
  /// CHECK-NEXT:                  ParallelMove liveness:82 moves:[r12->r13] liveness:82
  /// CHECK-NEXT: <<Field2:l\d+>>  InstanceFieldGet [<<This>>] field_name:ClassWithManyFields.o11 field_type:PrimNot generates_own_read_barrier:false liveness:82 locations:[rbp]->r12
  /// CHECK-NEXT:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>] liveness:84 locations:[rbx,r12]->invalid
  //
  /// CHECK:                       InstanceFieldSet [<<This>>,<<Cond>>] field_name:ClassWithManyFields.b field_type:PrimBoolean locations:[rbp,r13]->invalid

  protected final void a(Object arg) {
    boolean z = (arg != null);
    String str = z ? "null" : "non-null";
    this.aa = new ClassWithCtorWithManyArgs(
        this.o00, this.o01, this.o02, this.o03, this.o04, this.o05, this.o06,
        this.o07, this.o08, this.o09, this.o10, this.o11, this.o12, this.o13,
        this.o14, this.o15, this.o16, this.o17, this.o18, this.o19, this.o20,
        this.o21, this.o22, this.o23, this.o24, this.o25, this.o26, this.o27,
        this.o28, this.o29, this.o30, this.o31, this.o32, this.o33, this.o34,
        this.o35, this.o36, this.o37, this.o38, this.o39, this.o40, this.o41,
        this.o42, this.o43, this.o44, this.o45, this.o46, this.o47, this.o48,
        this.o49, this.o50);
    try {
      Object obj = this.m();
    } catch (Throwable e) {
      throw(e);
    }
    this.b = z;
  }

  static boolean test() {
    ClassWithManyFields c = new ClassWithManyFields();
    c.a(null);
    return c.b;
  }
}

class ClassWithCtorWithManyArgs {
  ClassWithCtorWithManyArgs(
      Object a00, Object a01, Object a02, Object a03, Object a04, Object a05, Object a06,
      Object a07, Object a08, Object a09, Object a10, Object a11, Object a12, Object a13,
      Object a14, Object a15, Object a16, Object a17, Object a18, Object a19, Object a20,
      Object a21, Object a22, Object a23, Object a24, Object a25, Object a26, Object a27,
      Object a28, Object a29, Object a30, Object a31, Object a32, Object a33, Object a34,
      Object a35, Object a36, Object a37, Object a38, Object a39, Object a40, Object a41,
      Object a42, Object a43, Object a44, Object a45, Object a46, Object a47, Object a48,
      Object a49, Object a50) {}
}
