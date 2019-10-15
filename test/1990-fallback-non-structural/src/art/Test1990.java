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

import java.util.Base64;
public class Test1990 {

  static class Transform {
    public void sayHi() {
      // Use lower 'h' to make sure the string will have a different string id
      // than the transformation (the transformation code is the same except
      // the actual printed String, which was making the test inacurately passing
      // in JIT mode when loading the string from the dex cache, as the string ids
      // of the two different strings were the same).
      // We know the string ids will be different because lexicographically:
      // "Goodbye" < "LTransform;" < "hello".
      System.out.println("hello");
    }
  }

  /**
   * base64 encoded class/dex file for
   * class Transform {
   *   public void sayHi() {
   *    System.out.println("Goodbye");
   *   }
   * }
   */
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
    "ZGV4CjAzNQBMrMJ2XmH3SntBgTsKWWskbrqgaPAA/1kwBAAAcAAAAHhWNBIAAAAAAAAAAGwDAAAV" +
    "AAAAcAAAAAkAAADEAAAAAgAAAOgAAAABAAAAAAEAAAQAAAAIAQAAAQAAACgBAADoAgAASAEAAJIB" +
    "AACaAQAAowEAAL0BAADNAQAA8QEAABECAAAoAgAAPAIAAFACAABkAgAAcwIAAH4CAACBAgAAhQIA" +
    "AJICAACYAgAAnQIAAKYCAACtAgAAtAIAAAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAgAAAAJAAAA" +
    "DAAAAAwAAAAIAAAAAAAAAA0AAAAIAAAAjAEAAAcABAAQAAAAAAAAAAAAAAAAAAAAEgAAAAQAAQAR" +
    "AAAABQAAAAAAAAAAAAAAAAAAAAUAAAAAAAAACgAAAFwDAAA7AwAAAAAAAAEAAQABAAAAgAEAAAQA" +
    "AABwEAMAAAAOAAMAAQACAAAAhAEAAAgAAABiAAAAGgEBAG4gAgAQAA4ABgAOAAgADngAAAAAAQAA" +
    "AAYABjxpbml0PgAHR29vZGJ5ZQAYTGFydC9UZXN0MTk5MCRUcmFuc2Zvcm07AA5MYXJ0L1Rlc3Qx" +
    "OTkwOwAiTGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0" +
    "aW9uL0lubmVyQ2xhc3M7ABVMamF2YS9pby9QcmludFN0cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0" +
    "OwASTGphdmEvbGFuZy9TdHJpbmc7ABJMamF2YS9sYW5nL1N5c3RlbTsADVRlc3QxOTkwLmphdmEA" +
    "CVRyYW5zZm9ybQABVgACVkwAC2FjY2Vzc0ZsYWdzAARuYW1lAANvdXQAB3ByaW50bG4ABXNheUhp" +
    "AAV2YWx1ZQB2fn5EOHsiY29tcGlsYXRpb24tbW9kZSI6ImRlYnVnIiwibWluLWFwaSI6MSwic2hh" +
    "LTEiOiI2MGRhNGQ2N2IzODFjNDI0Njc3NTdjNDlmYjZlNTU3NTZkODhhMmYzIiwidmVyc2lvbiI6" +
    "IjEuNy4xMi1kZXYifQACAgETGAECAwIOBAgPFwsAAAEBAICABMgCAQHgAgAAAAAAAAACAAAALAMA" +
    "ADIDAABQAwAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAVAAAAcAAAAAIAAAAJAAAA" +
    "xAAAAAMAAAACAAAA6AAAAAQAAAABAAAAAAEAAAUAAAAEAAAACAEAAAYAAAABAAAAKAEAAAEgAAAC" +
    "AAAASAEAAAMgAAACAAAAgAEAAAEQAAABAAAAjAEAAAIgAAAVAAAAkgEAAAQgAAACAAAALAMAAAAg" +
    "AAABAAAAOwMAAAMQAAACAAAATAMAAAYgAAABAAAAXAMAAAAQAAABAAAAbAMAAA==");

  public static void run() {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(Transform t) {
    // TODO Remove this once the class is structurally modifiable.
    System.out.println("Can structurally Redefine: " +
      Redefinition.isStructurallyModifiable(Transform.class));
    t.sayHi();
    Redefinition.doCommonStructuralClassRedefinition(Transform.class, DEX_BYTES);
    t.sayHi();
  }
}
