/*
 * Copyright (C) 2021 The Android Open Source Project
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
    testNeedsEnvironment();
    testNeedsBssEntryString();
    testNeedsBssEntryInvoke();
  }
  
  /// CHECK-START: void Main.testNeedsEnvironment() inliner (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Multi.$inline$NeedsEnvironmentMultiDex

  /// CHECK-START: void Main.testNeedsEnvironment() inliner (after)
  /// CHECK-NOT:   InvokeStaticOrDirect method_name:Multi.$inline$NeedsEnvironmentMultiDex

  /// CHECK-START: void Main.testNeedsEnvironment() inliner (after)
  /// CHECK:       StringBuilderAppend
  public static void testNeedsEnvironment() {
    Multi.$inline$NeedsEnvironmentMultiDex("abc");
  }

  /// CHECK-START: void Main.testNeedsBssEntryString() inliner (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Multi.$inline$NeedsBssEntryStringMultiDex

  /// CHECK-START: void Main.testNeedsBssEntryString() inliner (after)
  /// CHECK-NOT:   InvokeStaticOrDirect method_name:Multi.$inline$NeedsBssEntryStringMultiDex
  public static void testNeedsBssEntryString() {
    Multi.$inline$NeedsBssEntryStringMultiDex();
  }

  /// CHECK-START: void Main.testNeedsBssEntryInvoke() inliner (before)
  /// CHECK:       InvokeStaticOrDirect method_name:Multi.$inline$NeedsBssEntryInvokeMultiDex

  /// CHECK-START: void Main.testNeedsBssEntryInvoke() inliner (after)
  /// CHECK-NOT:   InvokeStaticOrDirect method_name:Multi.$inline$NeedsBssEntryInvokeMultiDex

  /// CHECK-START: void Main.testNeedsBssEntryInvoke() inliner (after)
  /// CHECK:       InvokeStaticOrDirect method_name:Multi.$noinline$InnerInvokeMultiDex
  public static void testNeedsBssEntryInvoke() {
    Multi.$inline$NeedsBssEntryInvokeMultiDex();
  }
}
