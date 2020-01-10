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

import dalvik.system.InMemoryDexClassLoader;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Base64;
import java.util.concurrent.CountDownLatch;
import java.util.function.Supplier;
import java.util.concurrent.atomic.*;
import java.lang.ref.*;

public class Test2009 {
  public static class Transform {
    public Transform() { }
  }
  /**
   * base64 encoded class/dex file for
   * package art;
   * public class Transform {
   *   public Transform() {
   *   }
   * }
   */
  private static final byte[] DEX_BYTES_INITIAL =
      Base64.getDecoder()
          .decode(
"ZGV4CjAzNQBMYVKB9B8EiEj/K5pUWVbEqHPGshupr2RkAgAAcAAAAHhWNBIAAAAAAAAAANABAAAG" +
"AAAAcAAAAAMAAACIAAAAAQAAAJQAAAAAAAAAAAAAAAIAAACgAAAAAQAAALAAAACUAQAA0AAAAPAA" +
"AAD4AAAACQEAAB0BAAAtAQAAMAEAAAEAAAACAAAABAAAAAQAAAACAAAAAAAAAAAAAAAAAAAAAQAA" +
"AAAAAAAAAAAAAQAAAAEAAAAAAAAAAwAAAAAAAAC/AQAAAAAAAAEAAQABAAAA6AAAAAQAAABwEAEA" +
"AAAOAAMADjwAAAAABjxpbml0PgAPTGFydC9UcmFuc2Zvcm07ABJMamF2YS9sYW5nL09iamVjdDsA" +
"DlRyYW5zZm9ybS5qYXZhAAFWAIwBfn5EOHsiY29tcGlsYXRpb24tbW9kZSI6ImRlYnVnIiwiaGFz" +
"LWNoZWNrc3VtcyI6ZmFsc2UsIm1pbi1hcGkiOjEsInNoYS0xIjoiZDFkNTFjMWNiM2U4NWFhMzBl" +
"MDBhNjgyMmNjYTgzYmJlMWRmZTk0NSIsInZlcnNpb24iOiIyLjAuMTMtZGV2In0AAAABAACBgATQ" +
"AQAAAAAAAAAMAAAAAAAAAAEAAAAAAAAAAQAAAAYAAABwAAAAAgAAAAMAAACIAAAAAwAAAAEAAACU" +
"AAAABQAAAAIAAACgAAAABgAAAAEAAACwAAAAASAAAAEAAADQAAAAAyAAAAEAAADoAAAAAiAAAAYA" +
"AADwAAAAACAAAAEAAAC/AQAAAxAAAAEAAADMAQAAABAAAAEAAADQAQAA");


  /**
   * base64 encoded class/dex file for
   * package art;
   * public static class Transform {
   *   public String greeting;
   *   public static String static_greeting;
   *
   *   public Transform() {
   *     greeting = "Hello";
   *   }
   *   public static String getGreetingStatic() { return static_greeting; }
   *   public String getGreeting() { return greeting; }
   * }
   */
  private static final byte[] DEX_BYTES =
      Base64.getDecoder()
          .decode(
"ZGV4CjAzNQC2h0jdvx3SGtxUfD59VBIXYlQJ+M8jnmhgAwAAcAAAAHhWNBIAAAAAAAAAAMACAAAN" +
"AAAAcAAAAAQAAACkAAAAAgAAALQAAAACAAAAzAAAAAQAAADcAAAAAQAAAPwAAABEAgAAHAEAAHwB" +
"AACEAQAAiwEAAI4BAACfAQAAswEAAMcBAADXAQAA2gEAAOcBAAD6AQAABAIAABUCAAADAAAABAAA" +
"AAUAAAAHAAAAAgAAAAIAAAAAAAAABwAAAAMAAAAAAAAAAAACAAoAAAAAAAIACwAAAAAAAQAAAAAA" +
"AAAAAAgAAAAAAAAACQAAAAEAAQAAAAAAAAAAAAEAAAABAAAAAAAAAAYAAAAAAAAApAIAAAAAAAAC" +
"AAEAAAAAAHYBAAADAAAAVBAAABEAAAABAAAAAAAAAHIBAAADAAAAYgABABEAAAACAAEAAQAAAGwB" +
"AAAIAAAAcBADAAEAGgABAFsQAAAOAAYADjxLAAkADgAKAA4AAAAGPGluaXQ+AAVIZWxsbwABTAAP" +
"TGFydC9UcmFuc2Zvcm07ABJMamF2YS9sYW5nL09iamVjdDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAO" +
"VHJhbnNmb3JtLmphdmEAAVYAC2dldEdyZWV0aW5nABFnZXRHcmVldGluZ1N0YXRpYwAIZ3JlZXRp" +
"bmcAD3N0YXRpY19ncmVldGluZwCMAX5+RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJkZWJ1ZyIsImhh" +
"cy1jaGVja3N1bXMiOmZhbHNlLCJtaW4tYXBpIjoxLCJzaGEtMSI6ImQxZDUxYzFjYjNlODVhYTMw" +
"ZTAwYTY4MjJjY2E4M2JiZTFkZmU5NDUiLCJ2ZXJzaW9uIjoiMi4wLjEzLWRldiJ9AAEBAgEBCQAB" +
"AIGABMwCAgm0AgEBnAIAAAAAAAANAAAAAAAAAAEAAAAAAAAAAQAAAA0AAABwAAAAAgAAAAQAAACk" +
"AAAAAwAAAAIAAAC0AAAABAAAAAIAAADMAAAABQAAAAQAAADcAAAABgAAAAEAAAD8AAAAASAAAAMA" +
"AAAcAQAAAyAAAAMAAABsAQAAAiAAAA0AAAB8AQAAACAAAAEAAACkAgAAAxAAAAEAAAC8AgAAABAA" +
"AAEAAADAAgAA");



  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest();
  }

  public static Class<?> MakeClass() throws Exception {
    return new InMemoryDexClassLoader(ByteBuffer.wrap(DEX_BYTES_INITIAL), Test2009.class.getClassLoader()).loadClass("art.Transform");
  }

  public static void doTest() throws Exception {
    // Make a transform
    Class<?> ifields = MakeClass();
    String res = NativeLocalGetIField(ifields.newInstance(), () -> {
      System.out.println("Doing redefinition for instance field");
      Redefinition.doCommonStructuralClassRedefinition(ifields, DEX_BYTES);
    });
    System.out.println("Result was " + res);
    Class<?> sfields = MakeClass();
    res = NativeLocalGetSField(sfields.newInstance(), () -> {
      System.out.println("Doing redefinition for static field");
      Redefinition.doCommonStructuralClassRedefinition(sfields, DEX_BYTES);
    });
    System.out.println("Result was " + res);
    Class<?> imeths = MakeClass();
    res = NativeLocalCallVirtual(imeths.newInstance(), () -> {
      System.out.println("Doing redefinition for instance method");
      Redefinition.doCommonStructuralClassRedefinition(imeths, DEX_BYTES);
    });
    System.out.println("Result was " + res);
    Class<?> smeths = MakeClass();
    res = NativeLocalCallStatic(smeths.newInstance(), () -> {
      System.out.println("Doing redefinition for static method");
      Redefinition.doCommonStructuralClassRedefinition(smeths, DEX_BYTES);
    });
    System.out.println("Result was " + res);
  }

  public static native String NativeLocalCallVirtual(Object t, Runnable thnk);
  public static native String NativeLocalCallStatic(Object t, Runnable thnk);
  public static native String NativeLocalGetIField(Object t, Runnable thnk);
  public static native String NativeLocalGetSField(Object t, Runnable thnk);
}
