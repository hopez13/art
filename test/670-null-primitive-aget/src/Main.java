/*
 * Copyright (C) 2015 The Android Open Source Project
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
    Object[] arguments = { };
    try {
      Class.forName("FailNullArrayGetToObject");
      throw new Error("Expected VerifyError");
    } catch (VerifyError e) {
      // Expected
    }

    Class<?> c = Class.forName("PassNullArray");
    String[] methods = { "B", "Z", "C", "S", "I", "F" };
    for (String i : methods) {
      Method m = c.getMethod("method" + i);
      try {
        m.invoke(null, arguments);
        throw new Error("Expected InvocationTargetException");
      } catch (InvocationTargetException e) {
        if (!(e.getCause() instanceof NullPointerException)) {
          throw new Error("Expected NullPointerException");
        }
      }
    }
  }
}
