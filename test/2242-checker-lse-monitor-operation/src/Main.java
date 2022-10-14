/*
 * Copyright (C) 2022 The Android Open Source Project
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

class TestClass {
  // static {
  //   sTestClassObj = new TestClass(-1, -2);
  // }
  TestClass() {
  }
  TestClass(int i, int j) {
    this.i = i;
    this.j = j;
  }
  int i;
  int j;
  // volatile int k;
  // TestClass next;
  // String str;
  // byte b;
  // static int si;
  // static TestClass sTestClassObj;
}

public class Main {
  public static void main(String[] args) {
    $noinline$assertEquals($noinline$testDifferentFieldsUnrealatedOperation(new TestClass(), new TestClass()), 3);
    $noinline$assertEquals($noinline$testDifferentFieldsMiddle(new TestClass(), new TestClass()), 3);
    $noinline$assertEquals($noinline$testRedundantStore(new TestClass()), 1);
  }

  public static void $noinline$assertEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // Reason LSE works in this group: Different fields shouldn't alias.

  /// CHECK-START: int Main.$noinline$testDifferentFieldsUnrealatedOperation(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testDifferentFieldsUnrealatedOperation(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testDifferentFieldsUnrealatedOperation(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  // Unrelated monitor operations shouldn't block LSE.
  static int $noinline$testDifferentFieldsUnrealatedOperation(TestClass obj1, TestClass obj2) {
    Main m = new Main();
    synchronized (m) {}

    obj1.i = 1;
    obj2.j = 2;
    int result = obj1.i + obj2.j;

    synchronized (m) {}

    return result;
  }

  /// CHECK-START: int Main.$noinline$testDifferentFieldsMiddle(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testDifferentFieldsMiddle(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testDifferentFieldsMiddle(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testDifferentFieldsMiddle(TestClass obj1, TestClass obj2) {
    Main m = new Main();
    obj1.i = 1;
    synchronized (m) { }
    obj2.j = 2;
    return obj1.i + obj2.j;
  }

  /// CHECK-START: int Main.$noinline$testRedundantStore(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testRedundantStore(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testRedundantStore(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: NullCheck
  /// CHECK-NOT: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldGet

  // Redundant store of the same value.
  static int $noinline$testRedundantStore(TestClass obj) {
    Main m = new Main();
    synchronized (m) {}

    obj.j = 1;
    obj.j = 1;
    return obj.j;
  }
}
