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

import art.*;

public class Main {
  // This is filled in by ASM before we get run.
  public static void main(String[] args) throws Exception {
    Redefinition.setTestConfiguration(Redefinition.STRUCTURAL_TRANSFORM);
    List<String> l1 = Arrays.asList("a", "b", "c", "d");
    ArrayList<String> l2 = new ArrayList<>();
    l2.add("1");
    l2.add("2");
    l2.add("3");
    l2.add("4");
    setupRedefineAbstractCollection();
    Redefinition.doCommonClassRetransformation(Collection.class);
  }
  public static native void setupRedefineAbstractCollection();
}
