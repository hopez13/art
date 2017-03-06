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

  // Note that this is testing we haven't intrinsified the byte[] arraycopy version.
  // Once we eventually start doing it, we will need to re-adjust this test.

  /// CHECK-START-X86: void Main.typedCopy(java.lang.Object, byte[]) disassembly (after)
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK-NOT:    call
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK:        call
  /// CHECK: ReturnVoid
  public static void typedCopy(Object o, byte[] foo) {
    System.arraycopy(o, 0, o, 0, 1);
    System.arraycopy(foo, 0, foo, 0, 1);
  }

  public static void untypedCopy(Object o, Object foo) {
    System.arraycopy(o, 0, o, 0, 1);
    System.arraycopy(foo, 0, foo, 0, 1);
  }

  // Test that we still do the optimization after inlining.

  /// CHECK-START-X86: void Main.untypedCopyCaller(java.lang.Object, byte[]) disassembly (after)
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK-NOT:    call
  /// CHECK: InvokeStaticOrDirect method_name:java.lang.System.arraycopy intrinsic:SystemArrayCopy
  /// CHECK:        call
  /// CHECK: ReturnVoid
  public static void untypedCopyCaller(Object o, byte[] array) {
    untypedCopy(o, array);
  }

  public static void main(String[] args) {
  }
}
