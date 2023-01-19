/*
 * Copyright (C) 2023 The Android Open Source Project
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
  public static void main(String[] args) {
    foo(new p1.A());
    foo(p1.Helper.getB());
    foo(p1.Helper.getC());
    foo(new SamePackage());

    bar(new p1.A());
    bar(p1.Helper.getB());
    bar(p1.Helper.getC());
    bar(new SamePackage());
  }

  public static void foo(p1.A obj) {
    // Fine to call a public method on a public class
    obj.method();

    try {
      obj.toBecomePackagePrivate();
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }
  }

  public static void bar(p1.A obj) {
    // Fine to access a public field.
    if (obj.field != 0) {
      throw new Error("Expected 0, got " + obj.field);
    }

    try {
      System.out.println(obj.toBecomePackagePrivateField);
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }
  }

  public static void foo(p1.B obj) {
    try {
      obj.method();
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }

    try {
      obj.toBecomePackagePrivate();
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }
  }

  public static void bar(p1.B obj) {
    try {
      System.out.println(obj.field);
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }

    try {
      System.out.println(obj.toBecomePackagePrivateField);
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }
  }

  public static void foo(SamePackage obj) {
    // Fine to call a public method.
    obj.method();

    try {
      obj.toBecomePackagePrivate();
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }
  }

  public static void bar(SamePackage obj) {
    // Fine to access a public field.
    if (obj.field != 0) {
      throw new Error("Expected 0, got " + obj.field);
    }

    try {
      System.out.println(obj.toBecomePackagePrivateField);
      throw new Error("Expected IllegalAccessError");
    } catch (IllegalAccessError e) {
    }
  }
}

class SamePackage extends p1.A {
}
