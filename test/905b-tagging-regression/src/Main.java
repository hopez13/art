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

import java.util.ArrayList;
import java.util.concurrent.TimeUnit;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[1]);
    doTest();
  }

  public static void doTest() throws Exception {
    // Use a list to ensure objects must be allocated.
    ArrayList<Object> l = new ArrayList<>(1000);

    setupObjectAllocationCallbacks();

    System.out.println("Tagging disabled");
    enableObjectTagging(false, false);
    for (int trial = 0; trial < 20; trial++) {
      long elapsedTime = System.nanoTime();
      long iterateTime = performAllocationsThenGc(l, 1000);
      elapsedTime = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - elapsedTime);

      System.out.println((elapsedTime - iterateTime) + "," + iterateTime);
    }

    System.out.println("Tagging enabled");
    for (int trial = 0; trial < 20; trial++) {
      // Tags unique across trials
      enableObjectTagging(true, false);
      long elapsedTime = System.nanoTime();
      long iterateTime = performAllocationsThenGc(l, 1000);
      elapsedTime = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - elapsedTime);
      enableObjectTagging(false, false);

      System.out.println((elapsedTime - iterateTime) + "," + iterateTime);
    }

    System.out.println("Tagging disabled");
    enableObjectTagging(false, false);
    for (int trial = 0; trial < 20; trial++) {
      long elapsedTime = System.nanoTime();
      long iterateTime = performAllocationsThenGc(l, 1000);
      elapsedTime = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - elapsedTime);
      System.out.println((elapsedTime - iterateTime) + "," + iterateTime);
    }

    System.out.println("Tagging enabled");
    for (int trial = 0; trial < 20; trial++) {
      // Reuse the same tags each trial
      enableObjectTagging(true, true);
      long elapsedTime = System.nanoTime();
      long iterateTime = performAllocationsThenGc(l, 1000);
      elapsedTime = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - elapsedTime);
      enableObjectTagging(false, false);

      System.out.println((elapsedTime - iterateTime) + "," + iterateTime);
    }
  }

  private static long performAllocationsThenGc(ArrayList<Object> l, int count) {
    for (int i = 0; i < count; ++i) {
      l.add(new Object());
    }
    l.clear();

    long elapsedTime = System.nanoTime();
    performIterateHeap();
    elapsedTime = TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - elapsedTime);

    Runtime.getRuntime().gc();

    return elapsedTime;
  }

  private static native void setupObjectAllocationCallbacks();
  private static native void enableObjectTagging(boolean enable, boolean resetTag);
  private static native void performIterateHeap();
}
