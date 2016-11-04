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


/*
 * A compile only test that checks that inlining a function that does not return works with
 * the LinearOrder computation.
 */

public class Main {

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static int inlinedFunction(int x, int y) {
    int result;
    if (x <= y) {
      result = 42;
    } else {
      while (true);
    }
    return result;
  }

  public static int callerLoop(int max_x, int max_y) {
    int total = 0;
    for (int x = 0; x < max_x; ++x) {
      total += inlinedFunction(x, max_y);
    }
    return total;
  }

  public static void main(String[] args) {
    assertIntEquals(42, callerLoop(1, 1));
  }
}
