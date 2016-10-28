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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    Class<?> c = Class.forName("ErrClass");
    Method m = c.getMethod("errMethod");

    // Loop enough to get the method JITed.
    long result = 0;
    for (int i = 0; i < 10000; i++) {
      try {
        result += (Long)m.invoke(null);
      } catch (InvocationTargetException e) {
        if (!(e.getCause() instanceof NullPointerException)) {
          throw e;
        }
      }
    }

    // Sleep for 2 seconds to give JIT a chance.
    // Not that we don't want ensureJitCompilationAttempt since that bypasses
    // sample collections. What we test here is that we don't collect samples
    // for non compilable methods and thus we don't end up with a JIT attempt.
    Thread.sleep(2000);

    // Invoke the method again to check everything is goes well.
    try {
      result += (Long)m.invoke(null);
    } catch (InvocationTargetException e) {
      if (!(e.getCause() instanceof NullPointerException)) {
        throw e;
      }
    }
    System.out.println(result);
  }

  private static native boolean ensureJitCompilationAttempt(Class<?> itf, String method_name);
}
