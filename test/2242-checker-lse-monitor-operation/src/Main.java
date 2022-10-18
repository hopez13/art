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
    // Forcing the synchronization to be there. Make sure the static variable is non-null.
    classForSync = new TestClass();

    // Different fields shouldn't alias.
    assertEquals($noinline$testDifferentFields(new TestClass(), new TestClass()), 3);
    assertEquals($noinline$testDifferentFieldsBlocking(new TestClass(), new TestClass()), 3);

    // Redundant store.
    assertEquals($noinline$testRedundantStore(new TestClass()), 2);
    assertEquals($noinline$testRedundantStoreBlocking(new TestClass()), 2);
    assertEquals($noinline$testRedundantStoreBlockingOnlyLoad(new TestClass()), 2);
    assertEquals($noinline$testRedundantStoreBlockingExit(new TestClass()), 2);

    // Set and merge values.
    assertEquals($noinline$testSetAndMergeValues(new TestClass(), true), 1);
    assertEquals($noinline$testSetAndMergeValues(new TestClass(), false), 2);
    assertEquals($noinline$testSetAndMergeValuesBlocking(new TestClass(), true), 1);
    assertEquals($noinline$testSetAndMergeValuesBlocking(new TestClass(), false), 2);

    // Allowing the synchronization to be removed

    // Different fields shouldn't alias.
    assertEquals(
            $noinline$testDifferentFieldsRemovedSynchronization(new TestClass(), new TestClass()),
            3);

    // Redundant store.
    assertEquals($noinline$testRedundantStoreRemovedSynchronization(new TestClass()), 2);

    // Set and merge values.
    assertEquals($noinline$testSetAndMergeValuesRemovedSynchronization(new TestClass(), true), 1);
    assertEquals($noinline$testSetAndMergeValuesRemovedSynchronization(new TestClass(), false), 2);

    // Removal of MonitorOperations work after inlining too. Also doesn't block regular LSE.
    assertEquals($noinline$testInlineSynchronizedMethod(new TestClass()), 2);
    assertEquals($noinline$testInlineMethodWithSynchronizedScope(new TestClass()), 2);
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
    synchronized (classForSync) {}

    obj1.i = 1;
    obj2.j = 2;
    int result = obj1.i + obj2.j;

    synchronized (classForSync) {}

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
    obj1.i = 1;
    obj2.j = 2;
    synchronized (classForSync) {}

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

  static int $noinline$testRedundantStore(TestClass obj) {
    synchronized (classForSync) {}
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
    // This store must be kept due to the monitor operation.
    obj.j = 1;
    synchronized (classForSync) {}
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
    // This store can be safely removed.
    obj.j = 1;
    obj.j = 2;
    synchronized (classForSync) {}

    // This load remains due to the monitor operation.
    return obj.j;
  }

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingExit(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingExit(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingExit(TestClass) load_store_elimination (after)
  /// CHECK:     InstanceFieldSet
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testRedundantStoreBlockingExit(TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testRedundantStoreBlockingExit(TestClass obj) {
    synchronized (classForSync) {
      // This store can be removed.
      obj.j = 0;
      // This store must be kept due to the monitor exit operation.
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
    if (b) {
      synchronized (classForSync) {}
      obj.i = 1;
    } else {
      synchronized (classForSync) {}
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
    if (b) {
      obj.i = 1;
    } else {
      obj.i = 2;
    }
    synchronized (classForSync) {}
    return obj.i;
  }

  /// CHECK-START: int Main.$noinline$testDifferentFieldsRemovedSynchronization(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testDifferentFieldsRemovedSynchronization(TestClass, TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testDifferentFieldsRemovedSynchronization(TestClass, TestClass) load_store_elimination (after)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testDifferentFieldsRemovedSynchronization(TestClass, TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testDifferentFieldsRemovedSynchronization(TestClass, TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testDifferentFieldsRemovedSynchronization(TestClass obj1, TestClass obj2) {
    Main m = new Main();

    obj1.i = 1;
    obj2.j = 2;
    synchronized (m) {}

    return obj1.i + obj2.j;
  }

  /// CHECK-START: int Main.$noinline$testRedundantStoreRemovedSynchronization(TestClass) load_store_elimination (before)
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldSet
  /// CHECK: InstanceFieldGet

  /// CHECK-START: int Main.$noinline$testRedundantStoreRemovedSynchronization(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testRedundantStoreRemovedSynchronization(TestClass) load_store_elimination (after)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testRedundantStoreRemovedSynchronization(TestClass) load_store_elimination (after)
  /// CHECK: InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testRedundantStoreRemovedSynchronization(TestClass) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testRedundantStoreRemovedSynchronization(TestClass obj) {
    Main m = new Main();

    obj.j = 1;
    synchronized (m) {}
    obj.j = 2;
    synchronized (m) {}

    return obj.j;
  }

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesRemovedSynchronization(TestClass, boolean) load_store_elimination (before)
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldGet
  /// CHECK-DAG: Return

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesRemovedSynchronization(TestClass, boolean) load_store_elimination (after)
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: InstanceFieldSet
  /// CHECK-DAG: Return

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesRemovedSynchronization(TestClass, boolean) load_store_elimination (before)
  /// CHECK-DAG: MonitorOperation kind:enter
  /// CHECK-DAG: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesRemovedSynchronization(TestClass, boolean) load_store_elimination (after)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesRemovedSynchronization(TestClass, boolean) load_store_elimination (after)
  /// CHECK: Phi

  /// CHECK-START: int Main.$noinline$testSetAndMergeValuesRemovedSynchronization(TestClass, boolean) load_store_elimination (after)
  /// CHECK-NOT: InstanceFieldGet

  static int $noinline$testSetAndMergeValuesRemovedSynchronization(TestClass obj, boolean b) {
    Main m = new Main();

    if (b) {
      obj.i = 1;
    } else {
      obj.i = 2;
    }
    synchronized (m) {}
    return obj.i;
  }

  synchronized int $inline$synchronizedSetter(TestClass obj) {
    obj.j = 1;
    obj.j = 2;
    return obj.j;
  }

  /// CHECK-START: int Main.$noinline$testInlineSynchronizedMethod(TestClass) inliner (before)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testInlineSynchronizedMethod(TestClass) inliner (after)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testInlineSynchronizedMethod(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testInlineSynchronizedMethod(TestClass) load_store_elimination (before)
  /// CHECK:     InstanceFieldSet
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testInlineSynchronizedMethod(TestClass) load_store_elimination (after)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testInlineSynchronizedMethod(TestClass) load_store_elimination (after)
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet
  static int $noinline$testInlineSynchronizedMethod(TestClass obj) {
    Main m = new Main();
    return m.$inline$synchronizedSetter(obj);
  }

  int $inline$SetterWithSynchronizedScope(TestClass obj) {
    synchronized (this) {
      obj.j = 1;
      obj.j = 2;
      return obj.j;
    }
  }

  /// CHECK-START: int Main.$noinline$testInlineMethodWithSynchronizedScope(TestClass) inliner (before)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testInlineMethodWithSynchronizedScope(TestClass) inliner (after)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testInlineMethodWithSynchronizedScope(TestClass) load_store_elimination (before)
  /// CHECK: MonitorOperation kind:enter
  /// CHECK: MonitorOperation kind:exit

  /// CHECK-START: int Main.$noinline$testInlineMethodWithSynchronizedScope(TestClass) load_store_elimination (before)
  /// CHECK:     InstanceFieldSet
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet

  /// CHECK-START: int Main.$noinline$testInlineMethodWithSynchronizedScope(TestClass) load_store_elimination (after)
  /// CHECK-NOT: MonitorOperation

  /// CHECK-START: int Main.$noinline$testInlineMethodWithSynchronizedScope(TestClass) load_store_elimination (after)
  /// CHECK:     InstanceFieldSet
  /// CHECK-NOT: InstanceFieldSet
  static int $noinline$testInlineMethodWithSynchronizedScope(TestClass obj) {
    Main m = new Main();
    return m.$inline$SetterWithSynchronizedScope(obj);
  }

  static TestClass classForSync;
}
