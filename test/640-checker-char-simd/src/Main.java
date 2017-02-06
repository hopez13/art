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
 * Functional tests for SIMD vectorization.
 */
public class Main {

  static char[] a;

  //
  // Arithmetic operations.
  //

  /// CHECK-START: void Main.add(int) loop_optimization (before)
  // /// CHECK-DAG: Phi loop:<<Loop:B\d+>> outer_loop:none
  // /// CHECK-DAG: Phi loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void Main.add(int) loop_optimization (after)
  // /// CHECK-DAG: Phi loop:<<Loop:B\d+>> outer_loop:none
  // /// CHECK-DAG: Phi loop:<<Loop>>      outer_loop:none
  static void add(int x) {
    for (int i = 0; i < 128; i++)
      a[i] += x;
  }

  static void sub(int x) {
    for (int i = 0; i < 128; i++)
      a[i] -= x;
  }

  static void mul(int x) {
    for (int i = 0; i < 128; i++)
      a[i] *= x;
  }

  static void div(int x) {
    for (int i = 0; i < 128; i++)
      a[i] /= x;
  }

  static void neg() {
    for (int i = 0; i < 128; i++)
      a[i] = (char) -a[i];
  }

  static void shl() {
    for (int i = 0; i < 128; i++)
      a[i] <<= 4;
  }

  static void sar() {
    for (int i = 0; i < 128; i++)
      a[i] >>= 2;
  }

  static void shr() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= 2;
  }

  static void sar31() {
    for (int i = 0; i < 128; i++)
      a[i] >>= 31;
  }

  static void shr31() {
    for (int i = 0; i < 128; i++)
      a[i] >>>= 31;
  }

  //
  // Loop bounds.
  //

  static void add() {
    for (int i = 1; i < 127; i++)
      a[i] += 11;
  }

  //
  // Test Driver.
  //

  public static void main(String[] args) {
    // Set up.
    a = new char[128];
    for (int i = 0; i < 128; i++) {
      a[i] = (char) i;
    }
    // Arithmetic operations.
    add(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i + 2, a[i], "add");
    }
    sub(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i, a[i], "sub");
    }
    mul(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i + i, a[i], "mul");
    }
    div(2);
    for (int i = 0; i < 128; i++) {
      expectEquals(i, a[i], "div");
    }
    neg();
    for (int i = 0; i < 128; i++) {
      expectEquals((char)-i, a[i], "neg");
    }
    // Loop bounds.
    add();
    expectEquals(0, a[0], "bounds0");
    for (int i = 1; i < 127; i++) {
      expectEquals((char)(11 - i), a[i], "bounds");
    }
    expectEquals((char)-127, a[127], "bounds127");
    // Shifts.
    for (int i = 0; i < 128; i++) {
      a[i] = (char) 0xffff;
    }
    shl();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0xfff0, a[i], "shl");
    }
    sar();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x3ffc, a[i], "sar");
    }
    shr();
    for (int i = 0; i < 128; i++) {
      expectEquals((char) 0x0fff, a[i], "shr");
      a[i] = (char) 0xffff;  // reset
    }
    sar31();
    for (int i = 0; i < 128; i++) {
      expectEquals(0, a[i], "sar31");
      a[i] = (char) 0xffff;  // reset
    }
    shr31();
    for (int i = 0; i < 128; i++) {
      expectEquals(0, a[i], "shr31");
    }
    // Done.
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result, String action) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result + " for " + action);
    }
  }
}
