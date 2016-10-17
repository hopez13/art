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

public class Main {
  public static void main(String[] args) throws Exception {
    // Use a list to ensure objects must be allocated.
    ArrayList<Object> l = new ArrayList<>(100);
    doTest(l);
  }

  public static void doTest(ArrayList<Object> l) throws Exception {

    try {
      for (int i = 0; i < 5; i++) {
        l.add(new Byte((byte)0));
        System.out.println(l.size());
        // Thread.sleep(500);
      }
    }
    catch (Exception ignored) {}
  }
}
