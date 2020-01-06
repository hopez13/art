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

import static art.Redefinition.CommonClassDefinition;

public class Test926 {
  // static class Transform {
  //   public void sayHi(Runnable r) {
  //     System.out.println("Hello - Transformed");
  //     r.run();
  //     System.out.println("Goodbye - Transformed");
  //   }
  // }
  private static CommonClassDefinition VALID_DEFINITION_T1 = new CommonClassDefinition(
      Transform926_1.class,
      test_926_transforms.Transform926_1.CLASS_BYTES,
      test_926_transforms.Transform926_1.DEX_BYTES);

  // static class Transform2 {
  //   public void sayHi(Runnable r) {
  //     System.out.println("Hello 2 - Transformed");
  //     r.run();
  //     System.out.println("Goodbye 2 - Transformed");
  //   }
  // }
  private static CommonClassDefinition VALID_DEFINITION_T2 = new CommonClassDefinition(
      Transform926_2.class,
      test_926_transforms.Transform926_2.CLASS_BYTES,
      test_926_transforms.Transform926_2.DEX_BYTES);

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform926_1(), new Transform926_2());
  }

  public static void doTest(final Transform926_1 t1, final Transform926_2 t2) throws Exception {
    t1.sayHi(() -> { t2.sayHi(() -> { System.out.println("Not doing anything here"); }); });
    t1.sayHi(() -> {
      t2.sayHi(() -> {
        System.out.println("transforming calling functions");
        Redefinition.doMultiClassRedefinition(VALID_DEFINITION_T1, VALID_DEFINITION_T2);
      });
    });
    t1.sayHi(() -> { t2.sayHi(() -> { System.out.println("Not doing anything here"); }); });
  }
}
