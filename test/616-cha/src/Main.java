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

class Main1 {
  String getName() {
    return "Main1";
  }

  void printError(String msg) {
    System.out.println(msg);
  }

  void foo(int i) {
    if (i != 1) {
      printError("error1");
    }
  }

  int getValue1() {
    return 1;
  }
  int getValue2() {
    return 2;
  }
  int getValue3() {
    return 3;
  }
  int getValue4() {
    return 4;
  }
  int getValue5() {
    return 5;
  }
  int getValue6() {
    return 6;
  }
}

class Main2 extends Main1 {
  String getName() {
    return "Main2";
  }

  void foo(int i) {
    if (i != 2) {
      printError("error2");
    }
  }
}

class Main3 extends Main1 {
  String getName() {
    return "Main3";
  }
}

public class Main {
  static Main1 sMain1;

  // sMain1.foo() will be always be Main1.foo() before Main2 is loaded/linked.
  // So sMain1.foo() can be devirtualized to Main1.foo() and be inlined.
  // After Dummy.createMain2() which links in Main2, live helper() on stack
  // should be deoptimized.
  static void helper(int i, boolean create) {
    sMain1.foo(sMain1.getClass() == Main1.class ? 1 : 2);
    if (i == 100 && create) {
      Dummy.createMain2();
    }
    if (!create) {
      // Make it easier to be suspended here by doing a sleep.
      try {
        Thread.sleep(1);
      } catch (Exception e) {
      }
    }
    // There should be a deoptimization here right after Main2 is linked in by
    // calling Dummy.createMain2().
    sMain1.foo(sMain1.getClass() == Main1.class ? 1 : 2);
  }

  static Main1[] sArray;

  // With CHA-based devirtualization, this method is compiled into a NullCheck
  // plus returning a constant since getValueX() all have single implementation
  // and m is a parameter so there is no need for a CHA guard.
  static long calcValue(Main1 m) {
    return m.getValue1()
        + m.getValue2() * 2
        + m.getValue3() * 3
        + m.getValue4() * 4
        + m.getValue5() * 5
        + m.getValue6() * 6;
  }

  static long testNoOverrideLoop(int count) {
    long sum = 0;
    for (int i=0; i<count; i++) {
      sum += calcValue(sArray[0]);
      sum += calcValue(sArray[1]);
      sum += calcValue(sArray[2]);
    }
    return sum;
  }

  static void testNoOverride() {
    sArray = new Main1[3];
    sArray[0] = new Main1();
    sArray[1] = new Main2();
    sArray[2] = new Main3();
    long sum = 0;
    for (int i=0; i<200000; i++) {
      testNoOverrideLoop(1);
    }
    // Give some time for jitting testNoOverrideLoop() to be completed.
    try {
      Thread.sleep(5000);
    } catch (Exception e) {
    }
    long t1 = System.currentTimeMillis();
    sum = testNoOverrideLoop(10000000);
    long t2 = System.currentTimeMillis();
    if (sum != 2730000000L) {
      System.out.println("Unexpected result.");
    }
  }

  public static void main(String[] args) throws Exception {
    // Allow some initial jitting of classpath methods to be finished.
    try {
      Thread.sleep(2000);
    } catch (Exception e) {}

    // sMain1 is an instance of Main1. Main2 hasn't bee loaded yet.
    sMain1 = new Main1();

    // Make helper start to be jitted.
    for (int i=0; i<100; i++) {
      helper(i, false);
    }
    // Give some time for jitting helper() to be completed.
    try {
      Thread.sleep(5000);
    } catch (Exception e) {}

    // Create another thread that also calls sMain1.foo().
    new Thread() {
      public void run() {
        for (int i=0; i<1000; i++) {
          helper(i, false);
        }
      }
    }.start();

    // This will create Main2 instance in the middle of helper().
    for (int i=0; i<200; i++) {
      helper(i, true);
    }

    testNoOverride();
  }
}

class Dummy {
  static void createMain2() {
    if (Main.sMain1.getClass() == Main1.class) {
      Main.sMain1 = new Main2();
    }
  }
}
