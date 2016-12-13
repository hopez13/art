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

public class Main {

  public static void main(String[] args) {
    Main main = new Main();

    System.out.println(main.testTwoAdjacentFieldGets());
    System.out.println(main.testThreeAdjacentFieldGets());
    System.out.println(main.testInterleavedFieldGets(new Main()));

    System.out.println(ClassWithManyFields.test());

    System.out.println("passed");
  }

  public Main() {
    o1 = new Object();
    o2 = new Object();
    o3 = new Object();
  }

  /*
   * Check graph without Baker read barriers, before and after the
   * gc_optimizer phase (performing no operation).
   */

  /// CHECK-START-IF(not envTrue('ART_USE_READ_BARRIER') or envEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (before)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false

  /// CHECK-START-IF(not envTrue('ART_USE_READ_BARRIER') or envEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false

  /*
   * Check graph before Baker read barrier merging.
   */

  /// CHECK-START-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (before)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:true

  /*
   * Check graph after Baker read barrier merging.
   */

  /// CHECK-START-ARM-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]

  /// CHECK-START-ARM64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]

  /// CHECK-START-X86-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]

  /// CHECK-START-X86_64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]

  /*
   * Check disassembly output after Baker read barrier merging
   * (without heap poisoning).
   */

  /// CHECK-START-ARM-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and not envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    ldr r0, [r1, #4]
  /// CHECK:                    add r1, r0, lsr #32
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    ldr r2, [r1, #8]
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    ldr r1, [r1, #12]
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]
  /// CHECK:                    lsrs r0, #29
  /// CHECK:                    bcs
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathARM
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg02
  /// CHECK:                    blx lr
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg01
  /// CHECK:                    blx lr
  /// CHECK:                    b

  /// CHECK-START-ARM64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and not envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    ldr w0, [x1, #4]
  /// CHECK:                    add x1, x1, x0, lsr #32
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    ldr w2, [x1, #8]
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    ldr w1, [x1, #12]
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]
  /// CHECK:                    tbnz w0, #28, #+0x{{\d+}}
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathARM64
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg02
  /// CHECK:                    blr lr
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg01
  /// CHECK:                    blr lr
  /// CHECK:                    b

  /// CHECK-START-X86-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and not envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    test [ecx + 7], 16
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    mov eax, [ecx + 8]
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    mov edx, [ecx + 12]
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]
  /// CHECK:                    jnz/ne +22 (0x00000042)
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathX86
  /// CHECK:                    call fs:[{{0x\d+}}]  ; pReadBarrierMarkReg00
  /// CHECK:                    call fs:[{{0x\d+}}]  ; pReadBarrierMarkReg02
  /// CHECK:                    jmp

  /// CHECK-START-X86_64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and not envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    test [ecx + 7], 16
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    mov eax, [ecx + 8]
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    mov edx, [ecx + 12]
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]
  /// CHECK:                    jnz/ne +22 (0x00000042)
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathX86_64
  /// CHECK:                    call gs:[{{\d+}}]  ; pReadBarrierMarkReg00
  /// CHECK:                    call gs:[{{\d+}}]  ; pReadBarrierMarkReg02
  /// CHECK:                    jmp

  /*
   * Check disassembly output after Baker read barrier merging
   * (with heap poisoning).
   */

  /// CHECK-START-ARM-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    ldr r0, [r1, #4]
  /// CHECK:                    add r1, r0, lsr #32
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    ldr r2, [r1, #8]
  /// CHECK:                    rsbs r2, #0
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    ldr r1, [r1, #12]
  /// CHECK:                    rsbs r1, #0
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]
  /// CHECK:                    lsrs r0, #29
  /// CHECK:                    bcs
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathARM
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg02
  /// CHECK:                    blx lr
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg01
  /// CHECK:                    blx lr
  /// CHECK:                    b

  /// CHECK-START-ARM64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    ldr w0, [x1, #4]
  /// CHECK:                    add x1, x1, x0, lsr #32
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    ldr w2, [x1, #8]
  /// CHECK:                    neg w2, w2
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    ldr w1, [x1, #12]
  /// CHECK:                    neg w1, w1
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]
  /// CHECK:                    tbnz w0, #28, #+0x{{\d+}}
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathARM64
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg02
  /// CHECK:                    blr lr
  /// CHECK:                    ldr lr, [tr, #{{\d+}}] ; pReadBarrierMarkReg01
  /// CHECK:                    blr lr
  /// CHECK:                    b

  /// CHECK-START-X86-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    test [ecx + 7], 16
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    mov eax, [ecx + 8]
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    mov edx, [ecx + 12]
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]
  /// CHECK:                    jnz/ne
  /// CHECK:                    neg eax
  /// CHECK:                    neg edx
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathX86
  /// CHECK:                    neg eax
  /// CHECK:                    neg edx
  /// CHECK:                    call fs:[{{0x\d+}}]  ; pReadBarrierMarkReg00
  /// CHECK:                    call fs:[{{0x\d+}}]  ; pReadBarrierMarkReg02
  /// CHECK:                    jmp

  /// CHECK-START-X86_64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP') and envTrue('ART_HEAP_POISONING')): boolean Main.testTwoAdjacentFieldGets() disassembly (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK:                    test [rsi + 7], 16
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                    mov eax, [rsi + 8]
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                    mov ecx, [rsi + 12]
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]
  /// CHECK:                    jnz/ne
  /// CHECK:                    neg eax
  /// CHECK:                    neg ecx
  /// CHECK: disasm           ReadBarrierMarkTwoReferencesSlowPathX86_64
  /// CHECK:                    neg eax
  /// CHECK:                    neg ecx
  /// CHECK:                    call gs:[{{\d+}}]  ; pReadBarrierMarkReg00
  /// CHECK:                    call gs:[{{\d+}}]  ; pReadBarrierMarkReg01
  /// CHECK:                    jmp

  boolean testTwoAdjacentFieldGets() {
    Object o1 = this.o1;
    Object o2 = this.o2;
    return o1 == o2;
  }

  /*
   * Check graph without Baker read barriers, before and after the
   * gc_optimizer phase (performing no operation).
   */

  /// CHECK-START-IF(not envTrue('ART_USE_READ_BARRIER') or envEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (before)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:false

  /// CHECK-START-IF(not envTrue('ART_USE_READ_BARRIER') or envEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:false

  /*
   * Check graph before Baker read barrier merging.
   */

  /// CHECK-START-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (before)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:true

  /*
   * Check graph after Baker read barrier merging.
   */

  /// CHECK-START-ARM-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:true

  /// CHECK-START-ARM64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<RBState:i\d+>> LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesExplicitRBState [<<RBState>>,<<Field1>>,<<Field2>>]
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:true

  /// CHECK-START-X86-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:true

  /// CHECK-START-X86_64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: {{v\d+}}         LoadReadBarrierState [<<Obj>>]
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Obj>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  MarkReferencesImplicitRBState [<<Field1>>,<<Field2>>]
  /// CHECK:                  InstanceFieldGet [<<Obj>>] field_name:Main.o3 generates_own_read_barrier:true

  boolean testThreeAdjacentFieldGets() {
    Object o1 = this.o1;
    Object o2 = this.o2;
    Object o3 = this.o3;
    return o1 == o2 && o2 == o3;
  }

  /*
   * Check graph without Baker read barriers, before and after the
   * gc_optimizer phase (performing no operation).
   */

  /// CHECK-START-IF(not envTrue('ART_USE_READ_BARRIER') or envEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testInterleavedFieldGets(Main) gc_optimizer (before)
  /// CHECK: <<This:l\d+>>    ParameterValue
  /// CHECK: <<Arg:l\d+>>     ParameterValue
  /// CHECK: <<That:l\d+>>    NullCheck [<<Arg>>]
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o2 generates_own_read_barrier:false

  /// CHECK-START-IF(not envTrue('ART_USE_READ_BARRIER') or envEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testInterleavedFieldGets(Main) gc_optimizer (after)
  /// CHECK: <<This:l\d+>>    ParameterValue
  /// CHECK: <<Arg:l\d+>>     ParameterValue
  /// CHECK: <<That:l\d+>>    NullCheck [<<Arg>>]
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o2 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o2 generates_own_read_barrier:false

  /*
   * Check that Baker read barrier on the interleaved InstanceFieldGet
   * instructions (on two different objects) are not merged.
   */

  /// CHECK-START-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testInterleavedFieldGets(Main) gc_optimizer (before)
  /// CHECK: <<This:l\d+>>    ParameterValue
  /// CHECK: <<Arg:l\d+>>     ParameterValue
  /// CHECK: <<That:l\d+>>    NullCheck [<<Arg>>]
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o1 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o1 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o2 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o2 generates_own_read_barrier:true

  /// CHECK-START-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testInterleavedFieldGets(Main) gc_optimizer (after)
  /// CHECK: <<This:l\d+>>    ParameterValue
  /// CHECK: <<Arg:l\d+>>     ParameterValue
  /// CHECK: <<That:l\d+>>    NullCheck [<<Arg>>]
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o1 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o1 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<That>>] field_name:Main.o2 generates_own_read_barrier:true
  /// CHECK:                  InstanceFieldGet [<<This>>] field_name:Main.o2 generates_own_read_barrier:true

  boolean testInterleavedFieldGets(Main that) {
    Object o11 = that.o1;
    Object o21 = this.o1;
    Object o12 = that.o2;
    Object o22 = this.o2;
    return o11 == o12 && o21 == o22;
  }

  private Object o1;
  private Object o2;
  private Object o3;
}
