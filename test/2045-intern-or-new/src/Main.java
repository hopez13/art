/*
 * Copyright (C) 2022 The Android Open Source Project
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

import dalvik.system.VMRuntime;
import java.util.Collections;
import java.util.IdentityHashMap;
import java.util.Set;

public class Main {
  final static int NUM_CALLS = 1000;

  public static void main(String[] args) throws Exception {
    System.out.println("Starting");
    System.loadLibrary(args[0]);
    long internOrNewPtr = VMRuntime.getNativeFunctionPtr("InternedOrNewStringUTF", 2);
    if (internOrNewPtr == 0) {
      System.out.println("Failed to get function ptr");
      return;
    }
    long bogusFnPtr1 = VMRuntime.getNativeFunctionPtr("InternedOrNewStringUTF", 1);
    if (bogusFnPtr1 != 0) {
      System.out.println("Ignored argument count");
    }
    long bogusFnPtr2 = VMRuntime.getNativeFunctionPtr("InternedOrNewXStringUTF", 2);
    if (bogusFnPtr2 != 0) {
      System.out.println("Got ptr for bogus function name");
    }
    Set<String> copies = Collections.newSetFromMap(new IdentityHashMap<>());
    for (int i = 0; i < NUM_CALLS; i++) {
      String anotherOne = runInternOrNew(internOrNewPtr);
      if (!"abcdEF42".equals(anotherOne)) {
        System.out.println("Call number " + i + " returned bad string: " + anotherOne);
      }
      copies.add(anotherOne);
      // Sleep for a little, so that the probability of running most calls during reference
      // processing becomes vanishingly small.
      try {
        Thread.sleep(1);
      } catch (InterruptedException e) {
        System.out.println("Unexpected exception");
      }
    }
    int distinct_copies = copies.size();
    if (distinct_copies < 1) {
      System.out.println("No strings in resulting set!");
    }
    if (distinct_copies > 3 * NUM_CALLS / 4) {
      System.out.println("Too many distinct strings: " + distinct_copies);
    }
    System.out.println("Finished");
  }

  private native static String runInternOrNew(long fn_ptr);
}
