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

package art;

import static test_917_transforms.Transform917.DEX_BYTES;
import static test_917_transforms.Transform917.CLASS_BYTES;
public class Test917 {
  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform917("Hello", "Goodbye"),
           new Transform917("start", "end"));
  }

  private static void printTransform(Transform917 t) {
    System.out.println("Result is " + t.getResult());
    System.out.println("take1 is " + t.take1);
    System.out.println("take2 is " + t.take2);
  }
  public static void doTest(Transform917 t1, Transform917 t2) {
    printTransform(t1);
    printTransform(t2);
    Redefinition.doCommonClassRedefinition(Transform917.class, CLASS_BYTES, DEX_BYTES);
    printTransform(t1);
    printTransform(t2);
  }
}
