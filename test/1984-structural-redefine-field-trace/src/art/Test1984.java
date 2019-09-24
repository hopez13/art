/*
 * Copyright (C) 2019 The Android Open Source Project
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

import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.util.Base64;

public class Test1984 {
  public static void notifyFieldModify(
      Executable method, long location, Class<?> f_klass, Object target, Field f, Object value) {
    System.out.println("method: " + method + "\tMODIFY: " + f + "\tSet to: " + value);
  }

  public static void notifyFieldAccess(
      Executable method, long location, Class<?> f_klass, Object target, Field f) {
    System.out.println("method: " + method + "\tACCESS: " + f);
  }

  public static class Transform {
    public static int count_down = 2;
    public static boolean boom = false;
    public static boolean tock = false;

    public static void tick() {
      boolean tocked = tock;
      tock = !tock;
      if (tocked) {
        count_down--;
      }
      if (count_down == 0) {
        boom = true;
      }
    }
  }

  /* Base64 encoded dex file for.
   * // NB The addition of aaa_INITIAL means the fields all have different offsets
   * public static class Transform {
   *   public static int aaa_INITIAL = 0;
   *   public static int count_down = 2;
   *   public static boolean boom = false;
   *   public static boolean tock = false;
   *   public static void tick() {
   *     boolean tocked = tock;
   *     tock = !tock;
   *     if (tocked) {
   *       count_down--;
   *     }
   *     if (count_down == 0) {
   *       boom = true;
   *     }
   *   }
   * }
   */
  public static final byte[] REDEFINED_DEX_BYTES =
      Base64.getDecoder()
          .decode(
              "ZGV4CjAzNQASr5eOzXkQ0031uQr6vFTMffcqFQBatO5YBAAAcAAAAHhWNBIAAAAAAAAAAKADAAAV"
                  + "AAAAcAAAAAgAAADEAAAAAQAAAOQAAAAEAAAA8AAAAAQAAAAQAQAAAQAAADABAAAIAwAAUAEAAOAB"
                  + "AADqAQAA8gEAAPUBAAAPAgAAHwIAAEMCAABjAgAAdwIAAIYCAACRAgAAlAIAAJcCAACkAgAAsQIA"
                  + "ALcCAADDAgAAyQIAAM8CAADVAgAA3AIAAAIAAAADAAAABAAAAAUAAAAGAAAABwAAAAoAAAALAAAA"
                  + "CgAAAAYAAAAAAAAAAQAAAAwAAAABAAcADgAAAAEAAAAPAAAAAQAHABIAAAABAAAAAAAAAAEAAAAB"
                  + "AAAAAQAAABEAAAAFAAAAAQAAAAEAAAABAAAABQAAAAAAAAAIAAAAkAMAAGMDAAAAAAAAAgAAAAAA"
                  + "AADKAQAACwAAABIAZwAAABIhZwECAGoAAQBqAAMADgAAAAEAAQABAAAA0QEAAAQAAABwEAMAAAAO"
                  + "AAIAAAAAAAAA1QEAABUAAABjAAMA3wEAAWoBAwASETgABwBgAAIAsRBnAAIAYAACADkABABqAQEA"
                  + "DgAIAA48PC0ABwAOAA0ADi1LPFtLLgAACDxjbGluaXQ+AAY8aW5pdD4AAUkAGExhcnQvVGVzdDE5"
                  + "ODQkVHJhbnNmb3JtOwAOTGFydC9UZXN0MTk4NDsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3Np"
                  + "bmdDbGFzczsAHkxkYWx2aWsvYW5ub3RhdGlvbi9Jbm5lckNsYXNzOwASTGphdmEvbGFuZy9PYmpl"
                  + "Y3Q7AA1UZXN0MTk4NC5qYXZhAAlUcmFuc2Zvcm0AAVYAAVoAC2FhYV9JTklUSUFMAAthY2Nlc3NG"
                  + "bGFncwAEYm9vbQAKY291bnRfZG93bgAEbmFtZQAEdGljawAEdG9jawAFdmFsdWUAdn5+RDh7ImNv"
                  + "bXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsIm1pbi1hcGkiOjEsInNoYS0xIjoiYjEwMDJlNjc0YTU2"
                  + "YWRmMjQ2ODFhN2MyYTQ0NTQ5NzYzYTE0ZjI3ZSIsInZlcnNpb24iOiIxLjYuMzAtZGV2In0AAgMB"
                  + "ExgCAgQCDQQJEBcJBAADAAAJAQkBCQEJAIiABNACAYGABPgCAQmQAwAAAAAAAgAAAFQDAABaAwAA"
                  + "hAMAAAAAAAAAAAAAAAAAAA8AAAAAAAAAAQAAAAAAAAABAAAAFQAAAHAAAAACAAAACAAAAMQAAAAD"
                  + "AAAAAQAAAOQAAAAEAAAABAAAAPAAAAAFAAAABAAAABABAAAGAAAAAQAAADABAAABIAAAAwAAAFAB"
                  + "AAADIAAAAwAAAMoBAAACIAAAFQAAAOABAAAEIAAAAgAAAFQDAAAAIAAAAQAAAGMDAAADEAAAAgAA"
                  + "AIADAAAGIAAAAQAAAJADAAAAEAAAAQAAAKADAAA=");

  public static void run() throws Exception {
    System.out.println("Dumping fields at start");
    for (Field f : Transform.class.getDeclaredFields()) {
      System.out.println(f.toString() + "=" + f.get(null));
    }
    Trace.disableTracing(Thread.currentThread());
    Trace.enableFieldTracing(
        Test1984.class,
        Test1984.class.getDeclaredMethod(
            "notifyFieldAccess",
            Executable.class,
            Long.TYPE,
            Class.class,
            Object.class,
            Field.class),
        Test1984.class.getDeclaredMethod(
            "notifyFieldModify",
            Executable.class,
            Long.TYPE,
            Class.class,
            Object.class,
            Field.class,
            Object.class),
        Thread.currentThread());
    for (Field f : Transform.class.getDeclaredFields()) {
      Trace.watchFieldAccess(f);
      Trace.watchFieldModification(f);
    }
    // count_down = 2
    Transform.tick(); // count_down = 2
    Transform.tick(); // count_down = 1
    System.out.println("REDEFINING TRANSFORM CLASS");
    long pre = System.currentTimeMillis();
    Redefinition.doCommonStructuralClassRedefinition(Transform.class, REDEFINED_DEX_BYTES);
    long post = System.currentTimeMillis();
    System.out.println("redef took " + (post - pre));
    Transform.tick(); // count_down = 1
    Transform.tick(); // count_down = 0
    System.out.println("Dumping fields at end");
    for (Field f : Transform.class.getDeclaredFields()) {
      System.out.println(f.toString() + "=" + f.get(null));
    }
    // Turn off tracing so we don't have to deal with print internals.
    Trace.disableTracing(Thread.currentThread());
  }
}
