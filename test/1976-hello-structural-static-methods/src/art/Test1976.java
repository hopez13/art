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

import java.lang.ref.*;
import java.lang.reflect.*;
import java.util.*;

public class Test1976 {

  // The fact that the target is having methods added makes it annoying to test since we cannot
  // initially call them. To work around this in a simple-ish way just use (non-structural)
  // redefinition to change the implementation of the caller of Transform1976 after redefining the
  // target.
  public static final class RunTransformMethods implements Runnable {
    public void run() {
      System.out.println("Saying everything!");
      Transform1976.sayEverything();
      System.out.println("Saying hi!");
      Transform1976.sayHi();
    }
  }

  /* Base64 encoded dex bytes of:
   * public static final class RunTransformMethods implements Runnable {
   *   public void run() {
   *    System.out.println("Saying everything!");
   *    Transform1976.sayEverything();
   *    System.out.println("Saying hi!");
   *    Transform1976.sayHi();
   *    System.out.println("Saying bye!");
   *    Transform1976.sayBye();
   *   }
   * }
   */
  public static final byte[] RUN_DEX_BYTES =
      Base64.getDecoder()
          .decode(
              "ZGV4CjAzNQCv3eV8jFcpSsqMGl1ZXRk2iraZO41D0TIgBQAAcAAAAHhWNBIAAAAAAAAAAFwEAAAc"
                  + "AAAAcAAAAAsAAADgAAAAAgAAAAwBAAABAAAAJAEAAAcAAAAsAQAAAQAAAGQBAACcAwAAhAEAAAYC"
                  + "AAAOAgAAMgIAAEICAABXAgAAewIAAJsCAACyAgAAxgIAANwCAADwAgAABAMAABkDAAAmAwAAOgMA"
                  + "AEYDAABVAwAAWAMAAFwDAABpAwAAbwMAAHQDAAB9AwAAggMAAIoDAACZAwAAoAMAAKcDAAABAAAA"
                  + "AgAAAAMAAAAEAAAABQAAAAYAAAAHAAAACAAAAAkAAAAKAAAAEAAAABAAAAAKAAAAAAAAABEAAAAK"
                  + "AAAAAAIAAAkABQAUAAAAAAAAAAAAAAAAAAAAFgAAAAIAAAAXAAAAAgAAABgAAAACAAAAGQAAAAUA"
                  + "AQAVAAAABgAAAAAAAAAAAAAAEQAAAAYAAAD4AQAADwAAAEwEAAAuBAAAAAAAAAEAAQABAAAA6gEA"
                  + "AAQAAABwEAYAAAAOAAMAAQACAAAA7gEAAB8AAABiAAAAGgENAG4gBQAQAHEAAwAAAGIAAAAaAQ4A"
                  + "biAFABAAcQAEAAAAYgAAABoBDABuIAUAEABxAAIAAAAOAAYADgAIAA54PHg8eDwAAQAAAAcAAAAB"
                  + "AAAACAAGPGluaXQ+ACJMYXJ0L1Rlc3QxOTc2JFJ1blRyYW5zZm9ybU1ldGhvZHM7AA5MYXJ0L1Rl"
                  + "c3QxOTc2OwATTGFydC9UcmFuc2Zvcm0xOTc2OwAiTGRhbHZpay9hbm5vdGF0aW9uL0VuY2xvc2lu"
                  + "Z0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ABVMamF2YS9pby9QcmludFN0"
                  + "cmVhbTsAEkxqYXZhL2xhbmcvT2JqZWN0OwAUTGphdmEvbGFuZy9SdW5uYWJsZTsAEkxqYXZhL2xh"
                  + "bmcvU3RyaW5nOwASTGphdmEvbGFuZy9TeXN0ZW07ABNSdW5UcmFuc2Zvcm1NZXRob2RzAAtTYXlp"
                  + "bmcgYnllIQASU2F5aW5nIGV2ZXJ5dGhpbmchAApTYXlpbmcgaGkhAA1UZXN0MTk3Ni5qYXZhAAFW"
                  + "AAJWTAALYWNjZXNzRmxhZ3MABG5hbWUAA291dAAHcHJpbnRsbgADcnVuAAZzYXlCeWUADXNheUV2"
                  + "ZXJ5dGhpbmcABXNheUhpAAV2YWx1ZQB2fn5EOHsiY29tcGlsYXRpb24tbW9kZSI6ImRlYnVnIiwi"
                  + "bWluLWFwaSI6MSwic2hhLTEiOiJhODM1MmYyNTQ4ODUzNjJjY2Q4ZDkwOWQzNTI5YzYwMDk0ZGQ4"
                  + "OTZlIiwidmVyc2lvbiI6IjEuNi4yMC1kZXYifQACAwEaGAECBAISBBkTFwsAAAEBAIGABIQDAQGc"
                  + "AwAAAAACAAAAHwQAACUEAABABAAAAAAAAAAAAAAAAAAAEAAAAAAAAAABAAAAAAAAAAEAAAAcAAAA"
                  + "cAAAAAIAAAALAAAA4AAAAAMAAAACAAAADAEAAAQAAAABAAAAJAEAAAUAAAAHAAAALAEAAAYAAAAB"
                  + "AAAAZAEAAAEgAAACAAAAhAEAAAMgAAACAAAA6gEAAAEQAAACAAAA+AEAAAIgAAAcAAAABgIAAAQg"
                  + "AAACAAAAHwQAAAAgAAABAAAALgQAAAMQAAACAAAAPAQAAAYgAAABAAAATAQAAAAQAAABAAAAXAQA"
                  + "AA==");

  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
  }

  private static final boolean PRINT_ID_NUM = false;

  public static void printRun(long id, Method m) {
    if (PRINT_ID_NUM) {
      System.out.println("Running method " + id + " " + m + " using JNI.");
    } else {
      System.out.println("Running method " + m + " using JNI.");
    }
  }

  public static void doTest() throws Exception {
    Runnable r = new RunTransformMethods();
    System.out.println("Running directly");
    r.run();
    System.out.println("Running reflective");
    Method[] methods = Transform1976.class.getDeclaredMethods();
    for (Method m : methods) {
      if (Modifier.isStatic(m.getModifiers())) {
        System.out.println("Reflectively invoking " + m);
        m.invoke(null);
      } else {
        System.out.println("Not invoking non-static method " + m);
      }
    }
    System.out.println("Running jni");
    long[] mids = getMethodIds(methods);
    callNativeMethods(Transform1976.class, mids);
    Redefinition.doCommonStructuralClassRedefinition(
        Transform1976.class, Transform1976.REDEFINED_DEX_BYTES);
    // Change RunTransformMethods to also call the 'runBye' method. No RI support so no classfile
    // bytes required.
    Redefinition.doCommonClassRedefinition(RunTransformMethods.class, new byte[] {}, RUN_DEX_BYTES);
    System.out.println("Running directly after redef");
    r.run();
    System.out.println("Running reflective after redef using old j.l.r.Method");
    for (Method m : methods) {
      if (Modifier.isStatic(m.getModifiers())) {
        System.out.println("Reflectively invoking " + m + " on old j.l.r.Method");
        m.invoke(null);
      } else {
        System.out.println("Not invoking non-static method " + m);
      }
    }
    System.out.println("Running reflective after redef using new j.l.r.Method");
    for (Method m : Transform1976.class.getDeclaredMethods()) {
      if (Modifier.isStatic(m.getModifiers())) {
        System.out.println("Reflectively invoking " + m + " on new j.l.r.Method");
        m.invoke(null);
      } else {
        System.out.println("Not invoking non-static method " + m);
      }
    }
    System.out.println("Running jni with old ids");
    callNativeMethods(Transform1976.class, mids);
    System.out.println("Running jni with new ids");
    callNativeMethods(Transform1976.class, getMethodIds(Transform1976.class.getDeclaredMethods()));
  }

  public static native long[] getMethodIds(Method[] m);

  public static native void callNativeMethods(Class<?> k, long[] smethods);
}
