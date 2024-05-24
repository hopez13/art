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
  static byte static_byte;
  static char static_char;
  static int static_int;
  static int unrelated_static_int;

  public static void assertByteEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertShortEquals(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertCharEquals(char expected, char result) {
    if (expected != result) {
      // Values are cast to int to display numeric values instead of
      // (UTF-16 encoded) characters.
      throw new Error("Expected: " + (int)expected + ", found: " + (int)result);
    }
  }

  /// CHECK-START: byte Main.getByte1() constant_folding (before)
  /// CHECK: TypeConversion
  /// CHECK: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte1() constant_folding (after)
  /// CHECK-NOT: TypeConversion
  /// CHECK-NOT: Add

  static byte getByte1() {
    int i = -2;
    int j = -3;
    return (byte)((byte)i + (byte)j);
  }

  /// CHECK-START: byte Main.getByte2() constant_folding (before)
  /// CHECK: TypeConversion
  /// CHECK: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte2() constant_folding (after)
  /// CHECK-NOT: TypeConversion
  /// CHECK-NOT: Add

  static byte getByte2() {
    int i = -100;
    int j = -101;
    return (byte)((byte)i + (byte)j);
  }

  /// CHECK-START: byte Main.getByte3() constant_folding (before)
  /// CHECK: TypeConversion
  /// CHECK: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte2() constant_folding (after)
  /// CHECK-NOT: TypeConversion
  /// CHECK-NOT: Add

  static byte getByte3() {
    long i = 0xabcdabcdabcdL;
    return (byte)((byte)i + (byte)i);
  }

  static byte byteVal = -1;
  static short shortVal = -1;
  static char charVal = 0xffff;
  static int intVal = -1;

  static byte[] byteArr = { 0 };
  static short[] shortArr = { 0 };
  static char[] charArr = { 0 };
  static int[] intArr = { 0 };

  static byte $noinline$getByte() {
    return byteVal;
  }

  static short $noinline$getShort() {
    return shortVal;
  }

  static char $noinline$getChar() {
    return charVal;
  }

  static int $noinline$getInt() {
    return intVal;
  }

  static boolean sFlag = true;

  /// CHECK-START: void Main.byteToShort() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void byteToShort() {
    shortArr[0] = 0;
    if (sFlag) {
      shortArr[0] = $noinline$getByte();
    }
  }

  /// CHECK-START: void Main.byteToChar() instruction_simplifier$before_codegen (after)
  /// CHECK: TypeConversion
  private static void byteToChar() {
    charArr[0] = 0;
    if (sFlag) {
      charArr[0] = (char)$noinline$getByte();
    }
  }

  /// CHECK-START: void Main.byteToInt() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void byteToInt() {
    intArr[0] = 0;
    if (sFlag) {
      intArr[0] = $noinline$getByte();
    }
  }

  /// CHECK-START: void Main.charToByte() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void charToByte() {
    byteArr[0] = 0;
    if (sFlag) {
      byteArr[0] = (byte)$noinline$getChar();
    }
  }

  /// CHECK-START: void Main.charToShort() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void charToShort() {
    shortArr[0] = 0;
    if (sFlag) {
      shortArr[0] = (short)$noinline$getChar();
    }
  }

  /// CHECK-START: void Main.charToInt() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void charToInt() {
    intArr[0] = 0;
    if (sFlag) {
      intArr[0] = $noinline$getChar();
    }
  }

  /// CHECK-START: void Main.shortToByte() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void shortToByte() {
    byteArr[0] = 0;
    if (sFlag) {
      byteArr[0] = (byte)$noinline$getShort();
    }
  }

  /// CHECK-START: void Main.shortToChar() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void shortToChar() {
    charArr[0] = 0;
    if (sFlag) {
      charArr[0] = (char)$noinline$getShort();
    }
  }

  /// CHECK-START: void Main.shortToInt() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void shortToInt() {
    intArr[0] = 0;
    if (sFlag) {
      intArr[0] = $noinline$getShort();
    }
  }

  /// CHECK-START: void Main.intToByte() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void intToByte() {
    byteArr[0] = 0;
    if (sFlag) {
      byteArr[0] = (byte)$noinline$getInt();
    }
  }

  /// CHECK-START: void Main.intToShort() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void intToShort() {
    shortArr[0] = 0;
    if (sFlag) {
      shortArr[0] = (short)$noinline$getInt();
    }
  }

  /// CHECK-START: void Main.intToChar() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: TypeConversion
  private static void intToChar() {
    charArr[0] = 0;
    if (sFlag) {
      charArr[0] = (char)$noinline$getInt();
    }
  }

  /// CHECK-START: int Main.$noinline$testWraparoundValue() load_store_elimination (before)
  /// CHECK:      <<Get:i\d+>>       StaticFieldGet field_name:Main.static_int field_type:Int32
  /// CHECK:      Return [<<Get>>]

  // Since the subtraction is done with int values, we have to have a type
  // conversion into byte to have the correct wraparound.

  /// CHECK-START: int Main.$noinline$testWraparoundValue() load_store_elimination (after)
  /// CHECK:      <<Sub:i\d+>>       Sub
  /// CHECK:      <<Conv:b\d+>>      TypeConversion [<<Sub>>]
  /// CHECK:      Return [<<Conv>>]
  public static int $noinline$testWraparoundValue() {
    // -120 - 24 is equal to 112 in `byte` due to wraparounds.
    static_byte = -120;
    static_byte -= 24;
    // Irrelevant code but needed to make LSE use loop Phi placeholders.
    for (int i = 1; i < 12; i++) {
        unrelated_static_int = 24;
    }
    static_int = static_byte;
    return static_int;
  }

  /// CHECK-START: int Main.$noinline$testNoWraparoundWithCast() load_store_elimination (before)
  /// CHECK:      <<Get:i\d+>>       StaticFieldGet field_name:Main.static_int field_type:Int32
  /// CHECK:      Return [<<Get>>]

  // Since the static_byte calculation is done with int values, we have to have a type
  // conversion into byte to have the correct value.

  /// CHECK-START: int Main.$noinline$testNoWraparoundWithCast() load_store_elimination (after)
  /// CHECK:      <<Add:i\d+>>       Add
  /// CHECK:      <<Conv1:b\d+>>     TypeConversion [<<Add>>]
  /// CHECK:      <<Conv2:a\d+>>     TypeConversion [<<Conv1>>]
  /// CHECK:      Return [<<Conv2>>]
  public static int $noinline$testNoWraparoundWithCast() {
    static_byte = -120;
    static_byte -= 8;
    // Irrelevant code but needed to make LSE use loop Phi placeholders.
    for (int i = 1; i < 12; i++) {
        unrelated_static_int = 24;
    }
    static_int = static_byte & 0xFF;
    return static_int;
  }

  // The first `static_int` Get is the one feeding into the conversion. The second
  // one is the one we return.
  /// CHECK-START: int Main.$noinline$testDoubleConversion(int) load_store_elimination (before)
  /// CHECK:      <<Get1:i\d+>>       StaticFieldGet field_name:Main.static_int field_type:Int32
  /// CHECK:      <<Get2:i\d+>>       StaticFieldGet field_name:Main.static_int field_type:Int32
  /// CHECK:      Return [<<Get2>>]

  // Since the static_byte calculation is done with int values, we have to have a type
  // conversion into byte to have the correct value.

  /// CHECK-START: int Main.$noinline$testDoubleConversion(int) load_store_elimination (after)
  /// CHECK:      <<Param:i\d+>>     ParameterValue
  /// CHECK:      <<Conv1:b\d+>>     TypeConversion [<<Param>>]
  /// CHECK:      <<Conv2:c\d+>>     TypeConversion [<<Conv1>>]
  /// CHECK:      Return [<<Conv2>>]
  public static int $noinline$testDoubleConversion(int value) {
    static_byte = (byte) value;
    // Irrelevant code but needed to make LSE use loop Phi placeholders.
    for (int i = 1; i < 12; i++) {
        unrelated_static_int = 24;
    }
    static_int = static_byte;
    static_char = (char) static_int;
    static_int = static_char;
    return static_int;
  }

  // TODO: Figure out why this gets miscompiled. The assert in line 366 will fail.
  public static int $noinline$testConversionWithCondition(int value, boolean cond) {
    static_int = value;
    // Irrelevant code but needed to make LSE use loop Phi placeholders.
    for (int i = 1; i < 12; i++) {
        if (cond) {
            static_byte = (byte) static_int;
            static_int = static_byte;
            // The loop Phi appears when we change the two lines above with:
            // static_int = (byte) static_int;
        }
    }
    return static_int;
}

  public static void main(String[] args) {
    assertByteEquals(getByte1(), (byte)-5);
    assertByteEquals(getByte2(), (byte)(-201));
    assertByteEquals(getByte3(), (byte)(0xcd + 0xcd));

    byteToShort();
    assertShortEquals(shortArr[0], (short)-1);
    byteToChar();
    assertCharEquals(charArr[0], (char)-1);
    byteToInt();
    assertIntEquals(intArr[0], -1);
    charToByte();
    assertByteEquals(byteArr[0], (byte)-1);
    charToShort();
    assertShortEquals(shortArr[0], (short)-1);
    charToInt();
    assertIntEquals(intArr[0], 0xffff);
    shortToByte();
    assertByteEquals(byteArr[0], (byte)-1);
    shortToChar();
    assertCharEquals(charArr[0], (char)-1);
    shortToInt();
    assertIntEquals(intArr[0], -1);
    intToByte();
    assertByteEquals(byteArr[0], (byte)-1);
    intToShort();
    assertShortEquals(shortArr[0], (short)-1);
    intToChar();
    assertCharEquals(charArr[0], (char)-1);

    assertIntEquals(112, $noinline$testWraparoundValue());
    assertIntEquals(128, $noinline$testNoWraparoundWithCast());

    int values[] = {
        Integer.MIN_VALUE, -129, -128, -1, 0, 127, 128, 65535, 65536, Integer.MAX_VALUE};
    int expected_values_testDoubleConversion[] = {
        0, 127, 65408, 65535, 0, 127, 65408, 65535, 0, 65535};
    for (int index = 0; index < values.length; ++index) {
      int value = $noinline$testDoubleConversion(values[index]);
      assertIntEquals(expected_values_testDoubleConversion[index], value);
    }

    // If the condition is false, the output should be equal to the input.
    int expected_values_testConversionWithCondition_false[] = values.clone();
    for (int index = 0; index < values.length; ++index) {
      int value = $noinline$testConversionWithCondition(values[index], false);
      int expected_value = expected_values_testConversionWithCondition_false[index];
      assertIntEquals(expected_value, value);
    }

    int expected_values_testConversionWithCondition_true[] = {
        0, 127, -128, -1, 0, 127, -128, -1, 0, -1};
    for (int index = 0; index < values.length; ++index) {
      int value = $noinline$testConversionWithCondition(values[index], true);
      int expected_value = expected_values_testConversionWithCondition_true[index];
      assertIntEquals(expected_value, value);
    }
  }
}
