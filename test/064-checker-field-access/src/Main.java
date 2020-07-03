/*
 * Copyright (C) 2020 The Android Open Source Project
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

  public static class Dummy {
    public Integer integerField;
    public int intField;
  }

  public static int intStaticField = 0;
  public static Integer integerStaticField = Integer.valueOf(5);
  public static String strStaticField = "";

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /**
   * Test that HInstanceFieldSet with a primitive type does not emit GCCardTableLoad.
   */

  /// CHECK-START-ARM64: void Main.instanceIntFieldSet(Main$Dummy, int) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:i\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,<<Parameter2>>]

  /// CHECK-START-ARM64: void Main.instanceIntFieldSet(Main$Dummy, int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:i\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,<<Parameter2>>]

  /// CHECK-START-ARM64: void Main.instanceIntFieldSet(Main$Dummy, int) instruction_simplifier_arm64 (after)
  //
  /// CHECK-NOT:                            GCCardTableLoad
  //

  public static void instanceIntFieldSet(Dummy obj, int val) {
    obj.intField = val;
  }

  /**
   * Test that HInstanceFieldSet with a reference type emits GCCardTableLoad.
   */

  /// CHECK-START-ARM64: void Main.instanceIntegerFieldSet(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,<<Parameter2>>]

  /// CHECK-START-ARM64: void Main.instanceIntegerFieldSet(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,<<Parameter2>>,<<GCCTabLoad>>]

  public static void instanceIntegerFieldSet(Dummy obj, Integer val) {
    obj.integerField = val;
  }

  /**
   * Test that HStaticFieldSet with a primitive type does not emit GCCardTableLoad.
   */

  /// CHECK-START-ARM64: void Main.staticIntFieldSet(int) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter:i\d+>>      ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         LoadClass [<<Method>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       StaticFieldSet [<<Object>>,<<Parameter>>]

  /// CHECK-START-ARM64: void Main.staticIntFieldSet(int) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter:i\d+>>      ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         LoadClass [<<Method>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       StaticFieldSet [<<Object>>,<<Parameter>>]

  /// CHECK-START-ARM64: void Main.staticIntFieldSet(int) instruction_simplifier_arm64 (after)
  //
  /// CHECK-NOT:                            GCCardTableLoad
  //

  public static void staticIntFieldSet(int val) {
    intStaticField = val;
  }


  /**
   * Test that HStaticFieldSet with a reference type emits GCCardTableLoad.
   */

  /// CHECK-START-ARM64: void Main.staticStrFieldSet(java.lang.String) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter:l\d+>>      ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         LoadClass [<<Method>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       StaticFieldSet [<<Object>>,<<Parameter>>]

  /// CHECK-START-ARM64: void Main.staticStrFieldSet(java.lang.String) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter:l\d+>>      ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         LoadClass [<<Method>>]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet:v\d+>>       StaticFieldSet [<<Object>>,<<Parameter>>,<<GCCTabLoad>>]

  public static void staticStrFieldSet(String val) {
    strStaticField = val;
  }

  /**
   * Check that the GC Card table load can be shared after GVN.
   */

  /// CHECK-START-ARM64: void Main.factorizationTest(Main$Dummy, java.lang.Integer, java.lang.String) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter3:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object1:l\d+>>        NullCheck [<<Parameter2>>]
  /// CHECK-DAG:    <<InstFieldGet1:i\d+>>  InstanceFieldGet [<<Object1>>]
  /// CHECK-DAG:    <<Object2:l\d+>>        NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object2>>,{{i\d+}}]
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object2>>,<<Parameter2>>]
  /// CHECK-DAG:    <<Class:l\d+>>          LoadClass [<<Method>>]
  /// CHECK-DAG:    <<StatFieldSet1:v\d+>>  StaticFieldSet [<<Class>>,<<Parameter2>>]
  /// CHECK-DAG:    <<StatFieldSet2:v\d+>>  StaticFieldSet [<<Class>>,<<Parameter3>>]

  /// CHECK-START-ARM64: void Main.factorizationTest(Main$Dummy, java.lang.Integer, java.lang.String) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter3:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object1:l\d+>>        NullCheck [<<Parameter2>>]
  /// CHECK-DAG:    <<InstFieldGet1:i\d+>>  InstanceFieldGet [<<Object1>>]
  /// CHECK-DAG:    <<Object2:l\d+>>        NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object2>>,{{i\d+}}]
  /// CHECK-DAG:    <<GCCTabLoad1:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object2>>,<<Parameter2>>,<<GCCTabLoad1>>]
  /// CHECK-DAG:    <<Class:l\d+>>          LoadClass [<<Method>>]
  /// CHECK-DAG:    <<GCCTabLoad2:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<StatFieldSet1:v\d+>>  StaticFieldSet [<<Class>>,<<Parameter2>>,<<GCCTabLoad2>>]
  /// CHECK-DAG:    <<GCCTabLoad3:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<StatFieldSet2:v\d+>>  StaticFieldSet [<<Class>>,<<Parameter3>>,<<GCCTabLoad3>>]

  /// CHECK-START-ARM64: void Main.factorizationTest(Main$Dummy, java.lang.Integer, java.lang.String) GVN$after_arch (after)
  /// CHECK-DAG:    <<Method:j\d+>>         CurrentMethod
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter3:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object1:l\d+>>        NullCheck [<<Parameter2>>]
  /// CHECK-DAG:    <<InstFieldGet1:i\d+>>  InstanceFieldGet [<<Object1>>]
  /// CHECK-DAG:    <<Object2:l\d+>>        NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object2>>,{{i\d+}}]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object2>>,<<Parameter2>>,<<GCCTabLoad>>]
  /// CHECK-DAG:    <<Class:l\d+>>          LoadClass [<<Method>>]
  /// CHECK-DAG:    <<StatFieldSet1:v\d+>>  StaticFieldSet [<<Class>>,<<Parameter2>>,<<GCCTabLoad>>]
  /// CHECK-DAG:    <<StatFieldSet2:v\d+>>  StaticFieldSet [<<Class>>,<<Parameter3>>,<<GCCTabLoad>>]

  public static void factorizationTest(Dummy obj, Integer i, String str) {
    obj.intField = i;
    obj.integerField = i;
    integerStaticField = i;
    strStaticField = str;
  }

  /**
   * Check that the how GC Card table load is shared after when in a loop.
   */

  /// CHECK-START-ARM64: void Main.loopFactorizationTest1(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<InstFieldGet:l\d+>>   InstanceFieldGet [<<Object>>]
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,{{l\d+}}]

  /// CHECK-START-ARM64: void Main.loopFactorizationTest1(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<InstFieldGet:l\d+>>   InstanceFieldGet [<<Object>>]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad>>]

  /// CHECK-START-ARM64: void Main.loopFactorizationTest1(Main$Dummy, java.lang.Integer) GVN$after_arch (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<InstFieldGet:l\d+>>   InstanceFieldGet [<<Object>>]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet:v\d+>>       InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad>>]
  public static void loopFactorizationTest1(Dummy obj, Integer i) {
    for (int j = 0; j < (i * i) / i; j++) {
       obj.integerField = j + obj.integerField;
    }
  }

  /**
   * Check that the GC Card table load is shared from outside the loop.
   */

  /// CHECK-START-ARM64: void Main.loopFactorizationTest2(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}}]
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}}]

  /// CHECK-START-ARM64: void Main.loopFactorizationTest2(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<GCCTabLoad1:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad1>>]
  /// CHECK-DAG:    <<GCCTabLoad2:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad2>>]

  /// CHECK-START-ARM64: void Main.loopFactorizationTest2(Main$Dummy, java.lang.Integer) GVN$after_arch (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad>>]
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad>>]
  public static void loopFactorizationTest2(Dummy obj, Integer i) {
    obj.integerField = 0;
    for (int j = 0; j < (i * i) / i; j++) {
       obj.integerField = j + obj.integerField;
    }
  }

  /**
   * Check that the GC Card Table load is shared between a non-conditional block and a conditional block.
   */

  /// CHECK-START-ARM64: void Main.condFactorizationTest(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (before)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}}]
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}}]

  /// CHECK-START-ARM64: void Main.condFactorizationTest(Main$Dummy, java.lang.Integer) instruction_simplifier_arm64 (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<GCCTabLoad1:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad1>>]
  /// CHECK-DAG:    <<GCCTabLoad2:j\d+>>    GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad2>>]

  /// CHECK-START-ARM64: void Main.condFactorizationTest(Main$Dummy, java.lang.Integer) GVN$after_arch (after)
  /// CHECK-DAG:    <<Parameter1:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Parameter2:l\d+>>     ParameterValue
  /// CHECK-DAG:    <<Object:l\d+>>         NullCheck [<<Parameter1>>]
  /// CHECK-DAG:    <<GCCTabLoad:j\d+>>     GCCardTableLoad
  /// CHECK-DAG:    <<FieldSet1:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad>>]
  /// CHECK-DAG:    <<FieldSet2:v\d+>>      InstanceFieldSet [<<Object>>,{{l\d+}},<<GCCTabLoad>>]
  public static void condFactorizationTest(Dummy obj, Integer i) {
    obj.integerField = i;
    if (i < 0) {
      obj.integerField = -1 * i;
    }
  }

  public static void main(String[] args) {
    Dummy dum = new Dummy();

    instanceIntFieldSet(dum, 1);
    instanceIntegerFieldSet(dum, Integer.valueOf(2));
    staticIntFieldSet(3);
    staticStrFieldSet("Hello World");

    factorizationTest(dum, Integer.valueOf(42), "foobar");
    assertIntEquals(integerStaticField, 42);
    assertIntEquals(dum.integerField, 42);
  }
}
