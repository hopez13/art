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

  // Method copied from 458-checker-instruct-simplification.
  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  // Method copied from 458-checker-instruct-simplification.
  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      throw new Error("Expected: " + (int)expected + ", found: " + (int)result);
    }
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTree(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTree(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTree(int a, int b, int c, int d) {
    int i1 = a + b;
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$LeftRightAddTree(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftRightAddTree(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftRightAddTree(int a, int b, int c, int d) {
    int i1 = a + b;
    int i2 = c + i1;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$RightLeftAddTree(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$RightLeftAddTree(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]
  public static int $noinline$RightLeftAddTree(int a, int b, int c, int d) {
    int i1 = a + b;
    int i2 = i1 + c;
    int i3 = d + i2;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$RightRightAddTree(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$RightRightAddTree(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$RightRightAddTree(int a, int b, int c, int d) {
    int i1 = a + b;
    int i2 = c + i1;
    int i3 = d + i2;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$RightRightAddConstTree(int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Const5:i\d+>>  IntConstant 5
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<Add1>>,<<Const5>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$RightRightAddConstTree(int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Const5:i\d+>>  IntConstant 5
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<DValue>>,<<Const5>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$RightRightAddConstTree(int a, int b, int c) {
    int i1 = a + b;
    int i2 = 5 + i1;
    int i3 = c + i2;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$RightRightLeftLeftLeftLeftAddTree(int, int, int, int, int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<EValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<FValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<GValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<HValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:     <<Add4:i\d+>>    Add [<<EValue>>,<<Add3>>]
  /// CHECK-DAG:     <<Add5:i\d+>>    Add [<<FValue>>,<<Add4>>]
  /// CHECK-DAG:     <<Add6:i\d+>>    Add [<<GValue>>,<<Add3>>]
  /// CHECK-DAG:     <<Add7:i\d+>>    Add [<<HValue>>,<<Add6>>]
  /// CHECK-DAG:     <<Add8:i\d+>>    Add [<<Add5>>,<<Add7>>]
  /// CHECK-DAG:                      Return [<<Add8>>]

  /// CHECK-START: int Main.$noinline$RightRightLeftLeftLeftLeftAddTree(int, int, int, int, int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<EValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<FValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<GValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<HValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<EValue>>,<<FValue>>]
  /// CHECK-DAG:     <<Add4:i\d+>>    Add [<<GValue>>,<<HValue>>]
  /// CHECK-DAG:     <<Add5:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:     <<Add6:i\d+>>    Add [<<Add5>>,<<Add3>>]
  /// CHECK-DAG:     <<Add7:i\d+>>    Add [<<Add5>>,<<Add4>>]
  /// CHECK-DAG:     <<Add8:i\d+>>    Add [<<Add6>>,<<Add7>>]
  /// CHECK-DAG:                      Return [<<Add8>>]

  public static int $noinline$RightRightLeftLeftLeftLeftAddTree(int a, int b, int c, int d,
                                                                int e, int f, int g, int h) {
    int i1 = a + b;
    int i2 = i1 + c;
    int i3 = i2 + d;
    int i4 = i3 + e;
    int i5 = i4 + f;
    int i6 = g + i3;
    int i7 = h + i6;
    int i8 = i7 + i5;
    return i8;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeImplicitConversion(int, int, char, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:c\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeImplicitConversion(int, int, char, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:c\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeImplicitConversion(int a, int b, char c, int d) {
    int i1 = a + b;
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: char Main.$noinline$LeftLeftAddTreeCharNarrowing(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>      Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>      DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+UXTH
  /// CHECK-DAG:     <<Add3:i\d+>>      DataProcWithShifterOp [<<DValue>>,<<Add2>>] kind:Add+UXTH
  /// CHECK-DAG:     <<CharAdd3:c\d+>>  TypeConversion [<<Add3>>]
  /// CHECK-DAG:                        Return [<<CharAdd3>>]

  /// CHECK-START: char Main.$noinline$LeftLeftAddTreeCharNarrowing(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>    ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>      Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>      DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+UXTH
  /// CHECK-DAG:     <<Add3:i\d+>>      DataProcWithShifterOp [<<DValue>>,<<Add2>>] kind:Add+UXTH
  /// CHECK-DAG:     <<CharAdd3:c\d+>>  TypeConversion [<<Add3>>]
  /// CHECK-DAG:                        Return [<<CharAdd3>>]

  public static char $noinline$LeftLeftAddTreeCharNarrowing(int a, int b, int c, int d) {
    char i1 = (char)(a + b);
    char i2 = (char)(i1 + c);
    char i3 = (char)(i2 + d);
    return i3;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeCharNarrowing2(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    DataProcWithShifterOp [<<AValue>>,<<BValue>>] kind:Add+UXTH
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeCharNarrowing2(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    DataProcWithShifterOp [<<AValue>>,<<BValue>>] kind:Add+UXTH
  /// CHECK-DAG:     <<Add2:i\d+>>    Add [<<CValue>>,<<DValue>>]
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<Add1>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeCharNarrowing2(int a, int b, int c, int d) {
    int i1 = a + (char)b;
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeShortNarrowing(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+SXTH
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeShortNarrowing(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+SXTH
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeShortNarrowing(int a, int b, int c, int d) {
    int i1 = (short)(a + b);
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeShortImplicit(short, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:s\d+>>     ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<MaskValue:i\d+>>  IntConstant 16776960
  /// CHECK-DAG:     <<MaskA:i\d+>>      And [<<AValue>>,<<MaskValue>>]
  /// CHECK-DAG:     <<Add1:i\d+>>       Add [<<BValue>>,<<MaskA>>]
  /// CHECK-DAG:     <<Add2:i\d+>>       Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>       Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                         Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeShortImplicit(short, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:s\d+>>     ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<MaskValue:i\d+>>  IntConstant 16776960
  /// CHECK-DAG:     <<MaskA:i\d+>>      And [<<AValue>>,<<MaskValue>>]
  /// CHECK-DAG:     <<Add1:i\d+>>       Add [<<BValue>>,<<CValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>       Add [<<MaskA>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>       Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                         Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeShortImplicit(short a, int b, int c, int d) {
    int mask_a = a & 0x00FFFF00;
    int i1 = mask_a + b;
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: long Main.$noinline$LeftLeftAddTreeIntNarrowing(long, long, long, long) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<BValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<CValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<DValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<Const1:j\d+>>        LongConstant 1
  /// CHECK-DAG:     <<Add1:j\d+>>          Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add1IntValue:i\d+>>  TypeConversion [<<Add1>>]
  /// CHECK-DAG:     <<Add2:j\d+>>          DataProcWithShifterOp [<<CValue>>,<<Add1IntValue>>] kind:Add+SXTW
  /// CHECK-DAG:     <<Add3:j\d+>>          Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:     <<Add4:j\d+>>          Add [<<Add3>>,<<Const1>>]
  /// CHECK-DAG:                            Return [<<Add4>>]

  /// CHECK-START: long Main.$noinline$LeftLeftAddTreeIntNarrowing(long, long, long, long) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<BValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<CValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<DValue:j\d+>>        ParameterValue
  /// CHECK-DAG:     <<Const1:j\d+>>        LongConstant 1
  /// CHECK-DAG:     <<Add1:j\d+>>          Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add1IntValue:i\d+>>  TypeConversion [<<Add1>>]
  /// CHECK-DAG:     <<Add2:j\d+>>          DataProcWithShifterOp [<<CValue>>,<<Add1IntValue>>] kind:Add+SXTW
  /// CHECK-DAG:     <<Add3:j\d+>>          Add [<<DValue>>,<<Const1>>]
  /// CHECK-DAG:     <<Add4:j\d+>>          Add [<<Add3>>,<<Add2>>]
  /// CHECK-DAG:                            Return [<<Add4>>]

  public static long $noinline$LeftLeftAddTreeIntNarrowing(long a, long b, long c, long d) {
    long i1 = (int)(a + b);
    long i2 = i1 + c;
    long i3 = i2 + d;
    long i4 = 1 + i3;
    return i4;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeByteNarrowing(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+SXTB
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeByteNarrowing(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+SXTB
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeByteNarrowing(int a, int b, int c, int d) {
    int i1 = (byte)(a + b);
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeByteImplicit(byte, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:b\d+>>     ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<MaskValue:i\d+>>  IntConstant -16776961
  /// CHECK-DAG:     <<MaskA:i\d+>>      And [<<AValue>>,<<MaskValue>>]
  /// CHECK-DAG:     <<Add1:i\d+>>       Add [<<BValue>>,<<MaskA>>]
  /// CHECK-DAG:     <<Add2:i\d+>>       Add [<<CValue>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>       Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                         Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeByteImplicit(byte, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:b\d+>>     ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>     ParameterValue
  /// CHECK-DAG:     <<MaskValue:i\d+>>  IntConstant -16776961
  /// CHECK-DAG:     <<MaskA:i\d+>>      And [<<AValue>>,<<MaskValue>>]
  /// CHECK-DAG:     <<Add1:i\d+>>       Add [<<BValue>>,<<CValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>       Add [<<MaskA>>,<<Add1>>]
  /// CHECK-DAG:     <<Add3:i\d+>>       Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                         Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeByteImplicit(byte a, int b, int c, int d) {
    int mask_a = a & 0xFF0000FF;
    int i1 = mask_a + b;
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeShortByteNarrowing(int, int, int, int) expression_dag_balancing (before)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+SXTB
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  /// CHECK-START: int Main.$noinline$LeftLeftAddTreeShortByteNarrowing(int, int, int, int) expression_dag_balancing (after)
  /// CHECK-DAG:     <<AValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<BValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<CValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<DValue:i\d+>>  ParameterValue
  /// CHECK-DAG:     <<Add1:i\d+>>    Add [<<AValue>>,<<BValue>>]
  /// CHECK-DAG:     <<Add2:i\d+>>    DataProcWithShifterOp [<<CValue>>,<<Add1>>] kind:Add+SXTB
  /// CHECK-DAG:     <<Add3:i\d+>>    Add [<<DValue>>,<<Add2>>]
  /// CHECK-DAG:                      Return [<<Add3>>]

  public static int $noinline$LeftLeftAddTreeShortByteNarrowing(int a, int b, int c, int d) {
    int i1 = (byte)(short)(a + b);
    int i2 = i1 + c;
    int i3 = i2 + d;
    return i3;
  }

  public static void main(String[] args) {
    assertIntEquals(10, $noinline$LeftLeftAddTree(1, 2, 3, 4));
    assertIntEquals(26, $noinline$LeftRightAddTree(5, 6, 7, 8));
    assertIntEquals(50, $noinline$RightLeftAddTree(13, 27, 6, 4));
    assertIntEquals(113, $noinline$RightRightAddTree(16, 24, 67, 6));
    assertIntEquals(112, $noinline$RightRightAddConstTree(16, 24, 67));
    assertIntEquals(46, $noinline$RightRightLeftLeftLeftLeftAddTree(1, 2, 3, 4, 5, 6, 7, 8));
    assertIntEquals(262, $noinline$LeftLeftAddTreeImplicitConversion(1, 2, (char)255, 4));
    assertCharEquals((char)10, $noinline$LeftLeftAddTreeCharNarrowing(1, 2, 3, 4));
    assertCharEquals((char)6, $noinline$LeftLeftAddTreeCharNarrowing(0xFF00, 0x00FF, 3, 4));
    assertIntEquals(65543, $noinline$LeftLeftAddTreeCharNarrowing2(0xFFFF, 1, 3, 4));
    assertIntEquals(7, $noinline$LeftLeftAddTreeShortNarrowing(1, 0xFFFF, 3, 4));
    assertIntEquals(0x00FFFF09, $noinline$LeftLeftAddTreeShortImplicit((short)0xFFFF, 2, 3, 4));
    assertLongEquals(7L, $noinline$LeftLeftAddTreeIntNarrowing(0L, -1L, 3L, 4L));
    assertLongEquals(7, $noinline$LeftLeftAddTreeByteNarrowing(1, 255, 3, 4));
    assertIntEquals(0xFF000108, $noinline$LeftLeftAddTreeByteImplicit((byte)0xFF, 2, 3, 4));
    assertLongEquals(8, $noinline$LeftLeftAddTreeShortByteNarrowing(-2, -1, 6, 5));
  }

}
