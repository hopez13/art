/*
 * Copyright (C) 2015 The Android Open Source Project
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

  static boolean doThrow = false;

  private void inlinedForNull(Iterable it) {
    if (it != null) {
      // We're not inlining throw at the moment.
      if (doThrow) { throw new Error(""); }
    }
  }

  private void inlinedForFalse(boolean value, Iterable it) {
    if (value) {
      // We're not inlining throw at the moment.
      if (doThrow) { throw new Error(""); }
    }
  }

  /// CHECK-START: void Main.testInlinedForFalseInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForFalseInlined(java.lang.Iterable) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      InvokeInterface

  public void testInlinedForFalseInlined(Iterable it) {
    inlinedForFalse(false, it);
  }

  /// CHECK-START: void Main.testInlinedForFalseNotInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForFalseNotInlined(java.lang.Iterable) inliner (after)
  /// CHECK:                          InvokeStaticOrDirect

  public void testInlinedForFalseNotInlined(Iterable it) {
    inlinedForFalse(true, it);
  }

  /// CHECK-START: void Main.testInlinedForNullInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForNullInlined(java.lang.Iterable) inliner (after)
  /// CHECK-NOT:                      InvokeStaticOrDirect
  /// CHECK-NOT:                      InvokeInterface

  public void testInlinedForNullInlined(Iterable it) {
    inlinedForNull(null);
  }

  /// CHECK-START: void Main.testInlinedForNullNotInlined(java.lang.Iterable) inliner (before)
  /// CHECK:                          InvokeStaticOrDirect

  /// CHECK-START: void Main.testInlinedForNullNotInlined(java.lang.Iterable) inliner (after)
  /// CHECK:                          InvokeStaticOrDirect

  public void testInlinedForNullNotInlined(Iterable it) {
    inlinedForNull(it);
  }

  /// CHECK-START: void Main.testIfCondStaticEvaluation(int[], boolean) dead_code_elimination$initial (before)
  /// CHECK-DAG: <<ParamCond:z\d+>> ParameterValue
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1
  /// CHECK-DAG:                    If [<<ParamCond>>]
  /// CHECK-DAG:                    If [<<ParamCond>>]
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const0>>]
  /// CHECK-DAG:                    If [<<ParamCond>>]
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const1>>]
  //
  /// CHECK-NOT:                    If [<<ParamCond>>]
  /// CHECK-NOT:                    ArraySet

  /// CHECK-START: void Main.testIfCondStaticEvaluation(int[], boolean) dead_code_elimination$initial (after)
  /// CHECK-DAG: <<ParamCond:z\d+>> ParameterValue
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0
  /// CHECK-DAG:                    If [<<ParamCond>>]
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const0>>]
  //
  /// CHECK-NOT:                    IntConstant 1
  /// CHECK-NOT:                    If [<<ParamCond>>]
  /// CHECK-NOT:                    ArraySet
  private static void testIfCondStaticEvaluation(int[] a, boolean f) {
    if (f) {
      if (f) {
        a[0] = 0;
      }
    } else {
      if (f) {
        a[0] = 1;
      }
    }
  }

  /// CHECK-START: void Main.testManualUnrollWithInvarExits(int[], boolean) dead_code_elimination$initial (before)
  /// CHECK-DAG: <<ParamCond:z\d+>> ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG:                    If [<<ParamCond>>]                        loop:none
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const0>>]   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                    If [<<ParamCond>>]                        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const0>>]   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    If [<<ParamCond>>]
  /// CHECK-NOT:                    ArraySet

  /// CHECK-START: void Main.testManualUnrollWithInvarExits(int[], boolean) dead_code_elimination$initial (after)
  /// CHECK-DAG: <<ParamCond:z\d+>> ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG:                    If [<<ParamCond>>]                        loop:none
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const0>>]   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                    ArraySet [{{l\d+}},{{i\d+}},<<Const0>>]   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    If [<<ParamCond>>]
  /// CHECK-NOT:                    ArraySet
  private static void testManualUnrollWithInvarExits(int[] a, boolean f) {
    if (f) {
      return;
    }
    a[0] = 0;
    for (int i = 1; i < a.length; i++) {
      if (f) {
        return;
      }
      a[i] = 0;
    }
  }

  static final int LENGTH = 4 * 1024;

  public static void main(String[] args) {
    Main m = new Main();
    Iterable it = new Iterable() {
      public java.util.Iterator iterator() { return null; }
    };
    m.testInlinedForFalseInlined(it);
    m.testInlinedForFalseNotInlined(it);
    m.testInlinedForNullInlined(it);
    m.testInlinedForNullNotInlined(it);

    int[] a = new int[LENGTH];
    m.testIfCondStaticEvaluation(a, true);
    m.testIfCondStaticEvaluation(a, false);
    m.testManualUnrollWithInvarExits(a, true);
    m.testManualUnrollWithInvarExits(a, false);
  }
}
