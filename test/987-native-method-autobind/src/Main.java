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

import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.HashMap;

public class Main {
  static {
    // Get Main's native methods all bound
    art.Main.bindAgentJNIForClass(Main.class);
  }

  private static final HashMap<Method, Long> SymbolMap = new HashMap<>();

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    setupNativeBindNotify();
    setNativeBindNotify(true);
    doTest();
  }

  private static void setNativeTransform(Method method, Long dest) {
    SymbolMap.put(method, dest);
  }

  private static void removeNativeTransform(Method method) {
    SymbolMap.remove(method);
  }

  /**
   * Notifies java that a native method bind has occurred and requests the new symbol to bind to.
   */
  public static long doNativeMethodBind(Method method) {
    long trans = (long)SymbolMap.getOrDefault(method, Long.valueOf(0));
    if (trans == 0) {
      System.out.println(method + " not being redefined");
    } else {
      System.out.println(method + " being redefined");
    }
    return trans;
  }

  public static void doTest() throws Exception {
    Method call_one_method = Transform.class.getDeclaredMethod(
        "callOneMethod", Class.class, Method.class, Method.class);
    Method say_hi_method = Main.class.getDeclaredMethod("sayHi");
    Method say_bye_method = Main.class.getDeclaredMethod("sayBye");

    // We haven't bound sayHi in any way yet.
    setNativeTransform(call_one_method, Transform.getPointerFor("callOtherMethod"));
    Transform.callOneMethod(Main.class, say_hi_method, say_bye_method);

    // Try intercepting direct set.
    setNativeTransform(call_one_method, Transform.getPointerFor("callBothMethods"));
    Transform.resetNativeImplementation();
    Transform.callOneMethod(Main.class, say_hi_method, say_bye_method);

    // Reset it back to the original. Make sure it all works.
    removeNativeTransform(call_one_method);
    Transform.resetNativeImplementation();
    Transform.callOneMethod(Main.class, say_hi_method, say_bye_method);
  }

  private static native void setNativeBindNotify(boolean enable);
  private static native void setupNativeBindNotify();

  // Targets hit by native code.
  private static void sayHi() {
    System.out.println("Hello");
  }

  private static void sayBye() {
    System.out.println("Goodbye");
  }
}
