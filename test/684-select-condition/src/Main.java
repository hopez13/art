/*
 * Copyright (C) 2018 The Android Open Source Project
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

  public static void main(String args[]) {
    doTest("1", "1.0");
    doTest("4", "2.0");
  }

  // This code is a reduced version of the original reproducer. The arm
  // code generator used to generate wrong code for it.
  public static void doTest(String s, String expected) {
    float a = (float)Integer.valueOf(s);
    a = a < 2.0f ? a : 2.0f;
    checkValue("" + a, expected);
  }

  public static void checkValue(String actual, String expected) {
    if (!expected.equals(actual)) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }
}
