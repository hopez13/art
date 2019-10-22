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
    public static void saySomething() {
      System.out.println("hello");
    }
  }

  /**
   * base64 encoded class/dex file for
   * static class Transform {
   *   public static void saySomething() {
   *    System.out.println("I say hello and " + sayGoodbye());
   *   }
   *   public static String sayGoodbye() {
   *    return "you say goodbye!";
   *   }
   * }
   */
  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
"ZGV4CjAzNQCV0LekDslEGFglxYgCw7HSyxVegIDjERswBQAAcAAAAHhWNBIAAAAAAAAAAGwEAAAc" +
"AAAAcAAAAAoAAADgAAAABAAAAAgBAAABAAAAOAEAAAgAAABAAQAAAQAAAIABAACQAwAAoAEAAC4C" +
"AAA2AgAASAIAAEsCAABPAgAAaQIAAHkCAACdAgAAvQIAANQCAADoAgAA/AIAABcDAAArAwAAOgMA" +
"AEUDAABIAwAATAMAAFkDAABhAwAAZwMAAGwDAAB1AwAAgQMAAI8DAACZAwAAoAMAALIDAAAEAAAA" +
"BQAAAAYAAAAHAAAACAAAAAkAAAAKAAAACwAAAAwAAAAPAAAAAgAAAAYAAAAAAAAAAwAAAAcAAAAo" +
"AgAADwAAAAkAAAAAAAAAEAAAAAkAAAAoAgAACAAEABQAAAAAAAIAAAAAAAAAAAAWAAAAAAACABcA" +
"AAAEAAMAFQAAAAUAAgAAAAAABwACAAAAAAAHAAEAEgAAAAcAAAAYAAAAAAAAAAAAAAAFAAAAAAAA" +
"AA0AAABcBAAAOQQAAAAAAAABAAAAAAAAABoCAAADAAAAGgAaABEAAAABAAEAAQAAABYCAAAEAAAA" +
"cBAEAAAADgAEAAAAAgAAAB4CAAAbAAAAYgAAAHEAAQAAAAwBIgIHAHAQBQACABoDAQBuIAYAMgBu" +
"IAYAEgBuEAcAAgAMAW4gAwAQAA4AAwAOAAgADgAFAA4BGg8AAAAAAQAAAAYABjxpbml0PgAQSSBz" +
"YXkgaGVsbG8gYW5kIAABTAACTEwAGExhcnQvVGVzdDE5OTAkVHJhbnNmb3JtOwAOTGFydC9UZXN0" +
"MTk5MDsAIkxkYWx2aWsvYW5ub3RhdGlvbi9FbmNsb3NpbmdDbGFzczsAHkxkYWx2aWsvYW5ub3Rh" +
"dGlvbi9Jbm5lckNsYXNzOwAVTGphdmEvaW8vUHJpbnRTdHJlYW07ABJMamF2YS9sYW5nL09iamVj" +
"dDsAEkxqYXZhL2xhbmcvU3RyaW5nOwAZTGphdmEvbGFuZy9TdHJpbmdCdWlsZGVyOwASTGphdmEv" +
"bGFuZy9TeXN0ZW07AA1UZXN0MTk5MC5qYXZhAAlUcmFuc2Zvcm0AAVYAAlZMAAthY2Nlc3NGbGFn" +
"cwAGYXBwZW5kAARuYW1lAANvdXQAB3ByaW50bG4ACnNheUdvb2RieWUADHNheVNvbWV0aGluZwAI" +
"dG9TdHJpbmcABXZhbHVlABB5b3Ugc2F5IGdvb2RieWUhAHZ+fkQ4eyJjb21waWxhdGlvbi1tb2Rl" +
"IjoiZGVidWciLCJtaW4tYXBpIjoxLCJzaGEtMSI6IjYwZGE0ZDY3YjM4MWM0MjQ2Nzc1N2M0OWZi" +
"NmU1NTc1NmQ4OGEyZjMiLCJ2ZXJzaW9uIjoiMS43LjEyLWRldiJ9AAICARkYAQIDAhEECBMXDgAA" +
"AwAAgIAEuAMBCaADAQnQAwAAAAAAAgAAACoEAAAwBAAAUAQAAAAAAAAAAAAAAAAAABAAAAAAAAAA" +
"AQAAAAAAAAABAAAAHAAAAHAAAAACAAAACgAAAOAAAAADAAAABAAAAAgBAAAEAAAAAQAAADgBAAAF" +
"AAAACAAAAEABAAAGAAAAAQAAAIABAAABIAAAAwAAAKABAAADIAAAAwAAABYCAAABEAAAAQAAACgC" +
"AAACIAAAHAAAAC4CAAAEIAAAAgAAACoEAAAAIAAAAQAAADkEAAADEAAAAgAAAEwEAAAGIAAAAQAA" +
"AFwEAAAAEAAAAQAAAGwEAAA=");



  public static void run() throws Exception {
    Redefinition.setTestConfiguration(Redefinition.Config.COMMON_REDEFINE);
    doTest(new Transform());
  }

  public static void doTest(Transform t) throws Exception {
    Transform.saySomething();
    Redefinition.doCommonStructuralClassRedefinition(Transform.class, DEX_BYTES);
    Transform.saySomething();
  }
}
