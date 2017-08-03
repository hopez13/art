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

/**
 * Regression test on ARM-scheduling/array-aliasing bug (b/64018485).
 */
public class Main {

  static void setFields(int[] fields) {
    if (fields == null || fields.length < 6)
      fields = new int[6];  // creates phi
    fields[5] = 127;
  }

  static void processFieldValues(int field0, int field1, int field2,
                                 int field3, int field4, int field5) {
    if (field5 != 127) {
      throw new Error("field = " + field5);
    } else if (field0 != 0) {
      processFieldValues(0, 0, 0, 0, 0, 0);  // disable inlining
    }
  }

  static int doit(int pass) {
    int[] fields = new int[6];
    for (; ; pass++) {
      setFields(fields);
      processFieldValues(fields[0], fields[1], fields[2],
                         fields[3], fields[4], fields[5]);
      if (pass == 0)
        break;
    }
    return fields[5];
  }

  static public void main(String[] args) {
    int r = doit(0);
    System.out.println("passed " + r);
  }
}
