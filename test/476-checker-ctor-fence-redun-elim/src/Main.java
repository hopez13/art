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

// Baseline class. This has no final fields, so there are no additional freezes
// in its constructor.
//
// The new-instance itself always has 1 freeze for the happens-before on the object header
// write (i.e. [obj.class = X] happens-before any access to obj).
//
// Total freezes for "new Base()": 1.
class Base {
  int w0;
  int w1;
  int w2;
  int w3;
}

// This has a final field in its constructor, so there must be a field freeze
// at the end of <init>.
//
// Total freezes for "new OneFinal()": 2.
class OneFinal extends Base {
  final int x;
  OneFinal(int x) {
    this.x = x;
  }
}

class TestOneFinal {
  // Initialize at least once before actual test.
  public static Object external;

  /// CHECK-START: void TestOneFinal.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]

  /// CHECK-START: void TestOneFinal.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  public void exercise() {
      Base b = new OneFinal(1);
      // 1 store, 2 freezes.

      // Stores to 'b' do not escape b.
      b.w0 = 1;
      b.w1 = 2;
      b.w2 = 3;

      // Publish the result to a global so that it is not LSE-eliminated.
      external = b;
  }
}

// This has a final field in its constructor, so there must be a field freeze
// at the end of <init>. The previous base class's freezes accumulate on top
// of this one.
//
// Total freezes for "new TwoFinal()": 3.
class TwoFinal extends OneFinal {
  final int y;
  TwoFinal(int x, int y) {
    super(x);
    this.y = y;
  }
}

// This has a final field in its constructor, so there must be a field freeze
// at the end of <init>. The previous base class's freezes accumulate on top
// of this one.
//
// Total freezes for "new ThreeFinal()": 4.
class ThreeFinal extends TwoFinal {
  final int z;
  ThreeFinal(int x, int y, int z) {
    super(x, y);
    this.z = z;
  }
}

class TestThreeFinal {
  // Initialize at least once before actual test.
  public static Object external;

  /// CHECK-START: void TestThreeFinal.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]

  /// CHECK-START: void TestThreeFinal.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  public void exercise() {
    Base b = new ThreeFinal(1, 1, 2);
    // 3 store, 4 freezes.

    // Stores to 'b' do not escape b.
    b.w0 = 3;

    // Publish the result to a global so that it is not LSE-eliminated.
    external = b;
  }
}

// Ensure "freezes" between multiple new-instances are optimized out.
class TestMultiAlloc {
  public static Object external;
  public static Object external2;

  /// CHECK-START: void TestMultiAlloc.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]

  /// CHECK-START: void TestMultiAlloc.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]
  public void exercise() {
    // 1 freeze
    Base b = new Base();
    // 1 freeze
    Base b2 = new Base();

    // Merge 2 freezes above into 1 constructor fence.
    external = b;
    external2 = b2;
  }
}

// Ensure "freezes" between multiple new-instances are optimized out.
class TestThreeFinalTwice {
  // Initialize at least once before actual test.
  public static Object external;
  public static Object external2;

  /// CHECK-START: void TestThreeFinalTwice.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]

  /// CHECK-START: void TestThreeFinalTwice.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]
  public void exercise() {
    Base b = new ThreeFinal(1, 1, 2);
    // 3 store, 4 freezes.

    // Stores to 'b' do not escape b.
    b.w0 = 3;

    Base b2 = new ThreeFinal(4, 5, 6);
    // 3 store, 4 freezes.

    // Stores to 'b2' do not escape b2.
    b2.w0 = 7;

    // Publish the result to a global so that it is not LSE-eliminated.
    // Publishing is done at the end to give freezes above a chance to merge.
    external = b;
    external2 = b2;
  }
}

class TestNonEscaping {
  // Prevent constant folding.
  static boolean test;

  static Object external;
  static Object external2;
  static Object external3;
  static Object external4;

  /// CHECK-START: void TestNonEscaping.invoke() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK:                          InvokeStaticOrDirect
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestNonEscaping.invoke() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          InvokeStaticOrDirect
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
  /// CHECK-NOT:                      ConstructorFence
  public void invoke() {
    Base b = new Base();

    // b cannot possibly escape into this invoke because it hasn't escaped onto the heap earlier,
    // and the invoke doesn't take it as a parameter.
    noEscape$noinline$();

    // Remove the Constructor Fence for b, merging into the fence for b2.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external = b;
    external2 = b2;
  }

  public static int[] array = new int[1];
  static Base base = new Base();

  /// CHECK-START: void TestNonEscaping.store() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ArraySet
  /// CHECK-DAG:                      StaticFieldSet
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestNonEscaping.store() constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
  /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
  /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>,<<NewInstance>>]
  /// CHECK-NOT:                        ConstructorFence
  public void store() {
    Base b = new Base();

    // Stores of inputs other than the fence target do not publish 'b'.
    array[0] = b.w0;  // aput
    external = array; // sput
    base.w0 = b.w0;   // iput

    // Remove the Constructor Fence for b, merging into the fence for b2.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external3 = b;
    external4 = b2;
  }

  public static void noEscape$noinline$() {
  }
}

class TestDontOptimizeAcrossBlocks {
  // Prevent constant folding.
  static boolean test;

  static Object external;
  static Object external2;
  static Object external3;

  /// CHECK-START: void TestDontOptimizeAcrossBlocks.exercise() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]

  /// CHECK-START: void TestDontOptimizeAcrossBlocks.exercise() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  /// CHECK-DAG:                      StaticFieldSet [<<External:l\d+>>,<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet [<<External2:l\d+>>,<<NewInstance2>>]
  public void exercise() {
    Base b = new Base();

    // Do not move constructor fence across this block, even though 'b' is not published yet.
    if (test) {
      external = null;
    }

    Base b2 = new Base();
    external = b2;
    external3 = b;
  }
}

class TestDontOptimizeAcrossEscape {
  // Prevent constant folding.
  static boolean test;

  static Object external;
  static Object external2;
  static Object external3;
  static Object external4;

  /// CHECK-START: void TestDontOptimizeAcrossEscape.invoke() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK:                          InvokeStaticOrDirect
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape.invoke() constructor_fence_redundancy_elimination (after)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK:                          InvokeStaticOrDirect
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence
  public void invoke() {
    Base b = new Base();
    // Do not optimize across invokes into which the fence target escapes.
    invoke$noinline$(b);

    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external = b;
    external2 = b2;
  }

  public static void invoke$noinline$(Object b) {
    // Even though 'b' does not escape this method, we conservatively assume all parameters
    // of an invoke escape.
  }

  public static Object[] array = new Object[10];
  static Base base = new Base();

  static class InstanceEscaper {
    public Object holder;
  }

  static InstanceEscaper instanceEscaper = new InstanceEscaper();

  /// CHECK-START: void TestDontOptimizeAcrossEscape.storeIput() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      InstanceFieldSet
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape.storeIput() constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
  /// CHECK:                            ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
  /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                        ConstructorFence
  public void storeIput() {
    Base b = new Base();

    // A store of 'b' into another instance will publish 'b'.
    instanceEscaper.holder = b;

    // Do not remove any constructor fences above.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external3 = b;
    external4 = b2;
  }

    /// CHECK-START: void TestDontOptimizeAcrossEscape.storeAput() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      ArraySet
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape.storeAput() constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
  /// CHECK:                            ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
  /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                        ConstructorFence
  public void storeAput() {
    Base b = new Base();

    // A store of 'b' into another array will publish 'b'.
    array[0] = b;  // aput

    // Do not remove any constructor fences above.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external3 = b;
    external4 = b2;
  }

  /// CHECK-START: void TestDontOptimizeAcrossEscape.storeSput() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      StaticFieldSet
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape.storeSput() constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
  /// CHECK:                            ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
  /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                        ConstructorFence
  public void storeSput() {
    Base b = new Base();

    // A store of 'b' into a static will publish 'b'.
    external = b;

    // Do not remove any constructor fences above.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external3 = b;
    external4 = b2;
  }

  /// CHECK-START: void TestDontOptimizeAcrossEscape.deopt() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      Deoptimize
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape.deopt() constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
  /// CHECK:                            ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
  /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                        ConstructorFence
  public void deopt() {
    Base b = new Base();

    // An array access generates a Deopt to avoid doing bounds check.
    array[0] = external;  // aput
    array[1] = external;  // aput
    array[2] = external;  // aput

    // Do not remove any constructor fences above.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external3 = b;
    external4 = b2;
  }

  /// CHECK-START: void TestDontOptimizeAcrossEscape.select() constructor_fence_redundancy_elimination (before)
  /// CHECK: <<NewInstance:l\d+>>     NewInstance
  /// CHECK:                          ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                      Select
  /// CHECK: <<NewInstance2:l\d+>>    NewInstance
  /// CHECK-DAG:                      ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                      ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape.select() constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<NewInstance:l\d+>>   NewInstance
  /// CHECK:                            ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG: <<NewInstance2:l\d+>>  NewInstance
  /// CHECK-DAG:                        ConstructorFence [<<NewInstance2>>]
  /// CHECK-NOT:                        ConstructorFence
  public void select() {
    Base b = new Base();

    boolean localTest = test;
    Object localExternal = external3;

    // Selecting 'b' creates an alias, which we conservatively assume escapes immediately.
    external = localTest ? b : localExternal;

    // Do not remove any constructor fences above.
    Base b2 = new Base();

    // Do not LSE-eliminate b,b2
    external3 = b;
    external4 = b2;
  }

  /// CHECK-START: void TestDontOptimizeAcrossEscape$MakeBoundType.<init>(int) constructor_fence_redundancy_elimination (before)
  /// CHECK-DAG: <<This:l\d+>>         ParameterValue
  /// CHECK-DAG: <<NewInstance:l\d+>>  NewInstance
  /// CHECK:                           ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                       BoundType
  /// CHECK-DAG:                       ConstructorFence [<<This>>]
  /// CHECK-NOT:                       ConstructorFence

  /// CHECK-START: void TestDontOptimizeAcrossEscape$MakeBoundType.<init>(int) constructor_fence_redundancy_elimination (after)
  /// CHECK-DAG: <<This:l\d+>>         ParameterValue
  /// CHECK-DAG: <<NewInstance:l\d+>>  NewInstance
  /// CHECK:                           ConstructorFence [<<NewInstance>>]
  /// CHECK-DAG:                       BoundType
  /// CHECK-DAG:                       ConstructorFence [<<This>>]
  /// CHECK-NOT:                       ConstructorFence
  static class MakeBoundType {
    final int abcdefgh;
    int x;

    MakeBoundType(int param) {
      abcdefgh = param;

      Base b = new Base();
      // constructor-fence(b)

      if (this instanceof MakeBoundTypeSub) {
        // Create a "BoundType(this)" which prevents
        // a merged constructor-fence(this, b)
        x = 1;
      } else {
        x = 2;
      }

      // publish(b).
      external = b;

      // constructor-fence(this)
    }
  }

  static class MakeBoundTypeSub extends MakeBoundType {
    MakeBoundTypeSub(int xyz) {
      super(xyz);
    }
  }
}

public class Main {
  public static void main(String[] args) {}
}
