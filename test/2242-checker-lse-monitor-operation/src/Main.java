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
  TestClass() {}
  int i;
  int j;
}

public class Main {
  public static void main(String[] args) {
    // Different fields shouldn't alias.
    assertEquals($noinline$testDifferentFields(new TestClass(), new TestClass()), 3);
    assertEquals($noinline$testDifferentFieldsBlocking(new TestClass(), new TestClass()), 3);

    // Redundant store of the same value.
    assertEquals($noinline$testRedundantStore(new TestClass()), 2);
    assertEquals($noinline$testRedundantStoreBlocking(new TestClass()), 2);
    assertEquals($noinline$testRedundantStoreBlockingOnlyLoad(new TestClass()), 2);
    assertEquals($noinline$testRedundantStoreExitDoesNotBlock(new TestClass()), 2);

    // Set and merge values.
    assertEquals($noinline$testSetAndMergeValues(new TestClass(), true), 1);
    assertEquals($noinline$testSetAndMergeValues(new TestClass(), false), 2);
    assertEquals($noinline$testSetAndMergeValuesBlocking(new TestClass(), true), 1);
    assertEquals($noinline$testSetAndMergeValuesBlocking(new TestClass(), false), 2);
  }

  public static void assertEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /// CHECK-START: int Main.$noinline$testDifferentFields(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testDifferentFields(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testDifferentFields(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testDifferentFields(TestClass, TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  // Unrelated monitor operations shouldn't block LSE.
  static int $noinline$testDifferentFields(TestClass obj1, TestClass obj2) {
    Main m = new Main();
    synchronized (m) {}

    obj1.i = 1;
    obj2.j = 2;
    int result = obj1.i + obj2.j;

    synchronized (m) {}

    return result;
  }

  /// CHECK-START: int Main.$noinline$testDifferentFieldsBlocking(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testDifferentFieldsBlocking(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testDifferentFieldsBlocking(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  // A synchronized operation blocks loads.
  static int $noinline$testDifferentFieldsBlocking(TestClass obj1, TestClass obj2) {
    Main m = new Main();

    obj1.i = 1;
    obj2.j = 2;
    synchronized (m) {}

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
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testRedundantStore(TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  // Redundant store of the same value.
  static int $noinline$testRedundantStore(TestClass obj) {
    Main m = new Main();
    synchronized (m) {}

    obj.j = 1;
    obj.j = 2;
    return obj.j;
  }

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlocking(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlocking(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlocking(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlocking(TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testRedundantStoreBlocking(TestClass obj) {
    Main m = new Main();

    // This store must be kept due to the monitor operation.
    obj.j = 1;
    synchronized (m) {}
    obj.j = 2;

    return obj.j;
  }

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingOnlyLoad(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingOnlyLoad(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingOnlyLoad(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingOnlyLoad(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldGet

  static int $noinline$testRedundantStoreBlockingOnlyLoad(TestClass obj) {
    Main m = new Main();

    // This store can be safely removed.
    obj.j = 1;
    obj.j = 2;
    synchronized (m) {}

    // This load remains due to the monitor operation.
    return obj.j;
  }

  /// CHECK-START: int Main.$noinline$testRedundantStoreExitDoesNotBlock(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testRedundantStoreExitDoesNotBlock(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testRedundantStoreExitDoesNotBlock(TestClass) load_store_elimination (after)
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testRedundantStoreExitDoesNotBlock(TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testRedundantStoreExitDoesNotBlock(TestClass obj) {
    Main m = new Main();

    synchronized (m) {
      obj.j = 1;
    }
    obj.j = 2;

    return obj.j;
  }


  /// CHECK-START: int Main.$noinline$testSetAndMergeValues(TestClass, boolean) load_store_elimination (before)
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldGet
  /// CHECK-DAG: Return

  /// CHECK-START: int Main.$noinline$testSetAndMergeValues(TestClass, boolean) load_store_elimination (after)
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: Return

  /// CHECK-START: int Main.$noinline$testSetAndMergeValues(TestClass, boolean) load_store_elimination (before)
  /// CHECK-DAG: MonitorOperation kind:enter
  /// CHECK-DAG: MonitorOperation kind:exit
  /// CHECK-DAG: MonitorOperation kind:enter
  /// CHECK-DAG: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testSetAndMergeValues(TestClass, boolean) load_store_elimination (after)
  /// CHECK: Phi

  /// CHECK-START: int Main.$noinline$testSetAndMergeValues(TestClass, boolean) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testSetAndMergeValues(TestClass obj, boolean b) {
    Main m = new Main();

    if (b) {
      synchronized (m) {}
      obj.i = 1;
    } else {
      synchronized (m) {}
      obj.i = 2;
    }
    return obj.i;
  }

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesBlocking(TestClass, boolean) load_store_elimination (before)
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldGet
  /// CHECK-DAG: Return

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesBlocking(TestClass, boolean) load_store_elimination (after)
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: Return

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesBlocking(TestClass, boolean) load_store_elimination (before)
  /// CHECK-DAG: MonitorOperation kind:enter
  /// CHECK-DAG: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesBlocking(TestClass, boolean) load_store_elimination (after)
  /// CHECK-NOT: Phi

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesBlocking(TestClass, boolean) load_store_elimination (after)
  /// CHECK: InstanceFieldGet

  static int $noinline$testSetAndMergeValuesBlocking(TestClass obj, boolean b) {
    Main m = new Main();

    if (b) {
      obj.i = 1;
    } else {
      obj.i = 2;
    }
    synchronized (m) {}
    return obj.i;
  }

}
