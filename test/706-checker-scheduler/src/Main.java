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

public class Main {

  static int static_variable = 0;

  // TODO(xueliang): enable checker test for this method.
  public static int arrayAccess() {
    int res = 0;
    int [] array = new int[10];
    for (int i = 0; i < 9; i++) {
      res += array[i];
      res += array[i + 1];
    }
    return res;
  }

  // TODO(xueliang): enable checker test for this method.
  public static int intDiv(int arg) {
    int res = 0;
    int tmp = arg;
    for (int i = 1; i < arg; i++) {
      tmp -= i;
      res = res / i;  // div-zero check barrier.
      static_variable++;
    }
    res += tmp;
    return res;
  }

  public static void main(String[] args) {
    if ((arrayAccess() + intDiv(10)) != -35) {
      System.out.println("FAIL");
    }
  }
}
