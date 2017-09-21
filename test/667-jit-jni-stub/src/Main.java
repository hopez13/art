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

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    if (isAotCompiled(Main.class, "hasJit")) {
      throw new Error("This test must be run with --no-prebuild --no-dex2oat!");
    }
    if (!hasJit()) {
      return;
    }
    int count = 0;
    do {
      for (int i = 0; i < 10000; ++i) {
        hasJit();
      }
      try {
        // Sleep to give a chance for the JIT to compile `hasJit` stub.
        Thread.sleep(100);
      } catch (Exception e) {
        // Ignore
      }
      if (++count == 50) {
        System.out.println("TIMEOUT");
        break;
      }
    } while (!isJitCompiled(Main.class, "hasJit"));
    // And try once again now that the method is compiled.
    hasJit();
  }

  public native static boolean isAotCompiled(Class<?> cls, String methodName);
  public native static boolean isJitCompiled(Class<?> cls, String methodName);
  private native static boolean hasJit();
}
