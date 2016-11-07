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

/**
 * Tests properties of some string operations represented by intrinsics.
 */
public class Main {

  static final String ABC = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  /// CHECK-START: int Main.liveIndexOf() dead_code_elimination$initial (before)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: int Main.liveIndexOf() dead_code_elimination$initial (after)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  static int liveIndexOf() {
    int k = ABC.length();
    for (char c = 'A'; c <= 'Z'; c++) {
      k += ABC.indexOf(c);
    }
    for (char c = 'A'; c <= 'Z'; c++) {
      k += ABC.indexOf(c, 4);
    }
    for (char c = 'A'; c <= 'Z'; c++) {
      k += ABC.indexOf("XY");
    }
    for (char c = 'A'; c <= 'Z'; c++) {
      k += ABC.indexOf("XY", 2);
    }
    return k;
  }

  /// CHECK-START: int Main.invariantBufferLiveIndexOf() licm (before)
  /// CHECK-DAG: InvokeVirtual loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.invariantBufferLiveIndexOf() licm (after)
  /// CHECK-DAG: InvokeVirtual loop:none
  /// CHECK-DAG: InvokeVirtual loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-NOT: InvokeVirtual loop:<<Loop>>      outer_loop:none
  static int invariantBufferLiveIndexOf() {
    int k = 0;
    StringBuffer b = new StringBuffer("F");
    for (char c = 'A'; c <= 'Z'; c++) {
      k += b.toString().indexOf(c);
    }
    return k;
  }

  /// CHECK-START: int Main.invariantBuilderLiveIndexOf() licm (before)
  /// CHECK-DAG: InvokeVirtual loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.invariantBuilderLiveIndexOf() licm (after)
  /// CHECK-DAG: InvokeVirtual loop:none
  /// CHECK-DAG: InvokeVirtual loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-NOT: InvokeVirtual loop:<<Loop>>      outer_loop:none
  static int invariantBuilderLiveIndexOf() {
    int k = 0;
    StringBuilder b = new StringBuilder("F");
    for (char c = 'A'; c <= 'Z'; c++) {
      k += b.toString().indexOf(c);
    }
    return k;
  }

  /// CHECK-START: int Main.deadIndexOf() dead_code_elimination$initial (before)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: int Main.deadIndexOf() dead_code_elimination$initial (after)
  /// CHECK-NOT: InvokeVirtual loop:{{B\d+}} outer_loop:none
  static int deadIndexOf() {
    int k = ABC.length();
    for (char c = 'A'; c <= 'Z'; c++) {
      int d = ABC.indexOf(c);
    }
    for (char c = 'A'; c <= 'Z'; c++) {
      int d = ABC.indexOf(c, 4);
    }
    for (char c = 'A'; c <= 'Z'; c++) {
      int d = ABC.indexOf("XY");
    }
    for (char c = 'A'; c <= 'Z'; c++) {
      int d = ABC.indexOf("XY", 2);
    }
    return k;
  }

  /// CHECK-START: int Main.invariantBufferDeadIndexOf() dead_code_elimination$initial (before)
  /// CHECK-DAG: InvokeVirtual loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.invariantBufferDeadIndexOf() dead_code_elimination$initial (after)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-NOT: InvokeVirtual loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: int Main.invariantBufferDeadIndexOf() licm (before)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: int Main.invariantBufferDeadIndexOf() licm (after)
  /// CHECK-DAG: InvokeVirtual loop:none
  /// CHECK-NOT: InvokeVirtual loop:{{B\d+}} outer_loop:none
  static int invariantBufferDeadIndexOf() {
    int k = 0;
    StringBuffer b = new StringBuffer("F");
    for (char c = 'A'; c <= 'Z'; c++) {
      int d = b.toString().indexOf(c);
    }
    return k;
  }

  /// CHECK-START: int Main.invariantBuilderDeadIndexOf() dead_code_elimination$initial (before)
  /// CHECK-DAG: InvokeVirtual loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: InvokeVirtual loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.invariantBuilderDeadIndexOf() dead_code_elimination$initial (after)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  /// CHECK-NOT: InvokeVirtual loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: int Main.invariantBuilderDeadIndexOf() licm (before)
  /// CHECK-DAG: InvokeVirtual loop:{{B\d+}} outer_loop:none
  //
  /// CHECK-START: int Main.invariantBuilderDeadIndexOf() licm (after)
  /// CHECK-DAG: InvokeVirtual loop:none
  /// CHECK-NOT: InvokeVirtual loop:{{B\d+}} outer_loop:none
  static int invariantBuilderDeadIndexOf() {
    int k = 0;
    StringBuilder b = new StringBuilder("F");
    for (char c = 'A'; c <= 'Z'; c++) {
      int d = b.toString().indexOf(c);
    }
    return k;
  }

  public static void main(String[] args) {
    expectEquals(1862, liveIndexOf());
    expectEquals(-25, invariantBufferLiveIndexOf());
    expectEquals(-25, invariantBuilderLiveIndexOf());

    expectEquals(26, deadIndexOf());
    expectEquals(0, invariantBufferDeadIndexOf());
    expectEquals(0, invariantBuilderDeadIndexOf());

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
