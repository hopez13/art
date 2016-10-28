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

import java.util.Arrays;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[1]);

    doTest();
  }

  public static void doTest() throws Exception {
    foo(4, 0, 25, null);

    System.out.println(Arrays.toString(storedResult));
  }

  private static int foo(int x, int start, int max, Object waitFor) {
    bar(x, start, max, waitFor);
    return 0;
  }

  private static long bar(int x, int start, int max, Object waitFor) {
    baz(x, start, max, waitFor);
    return 0;
  }

  private static Object baz(int x, int start, int max, Object waitFor) {
    if (x == 0) {
      printOrWait(start, max, waitFor);
    } else {
      foo(x - 1, start, max, waitFor);
    }
    return null;
  }

  private static void printOrWait(int start, int max, Object waitFor) {
    if (waitFor == null) {
      storedResult = getStackTrace(Thread.currentThread(), start, max);
    } else {
      synchronized (waitFor) {
        try {
          waitFor.wait();
        } catch (Throwable t) {
          throw new RuntimeException(t);
        }
      }
    }
  }

  static String[] storedResult;

  private static native String[] getStackTrace(Thread thread, int start, int max);
}
