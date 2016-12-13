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

public class Main {

  public static void main(String[] args) {
    Main main = new Main();

    System.out.println(main.testTwoAdjacentFieldGets());
    System.out.println(main.testThreeAdjacentFieldGets());
    System.out.println(main.testInterleavedFieldGets(new Main()));

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

  /// CHECK-START-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testTwoAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<Updated:l\d+>> UpdateFields [<<Obj>>] field1_name:Main.o1 field2_name:Main.o2
  /// CHECK: <<Field1:l\d+>>  InstanceFieldGet [<<Updated>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK: <<Field2:l\d+>>  InstanceFieldGet [<<Updated>>] field_name:Main.o2 generates_own_read_barrier:false

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

  /// CHECK-START-ARM64-IF(envTrue('ART_USE_READ_BARRIER') and envNotEquals('ART_READ_BARRIER_TYPE', 'TABLELOOKUP')): boolean Main.testThreeAdjacentFieldGets() gc_optimizer (after)
  /// CHECK: <<Obj:l\d+>>     ParameterValue
  /// CHECK: <<Updated:l\d+>> UpdateFields [<<Obj>>] field1_name:Main.o1 field2_name:Main.o2
  /// CHECK:                  InstanceFieldGet [<<Updated>>] field_name:Main.o1 generates_own_read_barrier:false
  /// CHECK:                  InstanceFieldGet [<<Updated>>] field_name:Main.o2 generates_own_read_barrier:false
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
