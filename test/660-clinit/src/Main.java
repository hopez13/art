/*
 * Copyright 2016 The Android Open Source Project
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

import java.util.*;

public class Main {

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);

    if (!checkAppImageLoaded()) {
      System.out.println("AppImage not loaded.");
    }

    if (!checkAppImageContains(ClInit.class)) {
      System.out.println("ClInit class is not in app image!");
    }

    ShouldInit(ClInit.class);
    ShouldInit(A.class);
    ShouldNotInit(D.class);
    ShouldInit(E.class);
    ShouldNotInit(ClinitE.class);
    ShouldInit(QC.class);

    ShouldNotInit(G.class);
    ShouldNotInit(Gs.class);

    Gs gs = new Gs();

    A x = new A();
    System.out.println("A.a: " + A.a);

    ClInit c = new ClInit();
    int aa = c.a;

    System.out.println("X: " + c.getX());
    System.out.println("Y: " + c.getY());
    System.out.println("str: " + c.str);
    System.out.println("ooo: " + c.ooo);
    System.out.println("Z: " + c.getZ());
    System.out.println("A: " + c.getA());
    System.out.println("AA: " + aa);

    if (c.a != 101) {
      System.out.println("a != 101");
    }

    try {
      ClinitE e = new ClinitE();
    } catch (Error err) { }

    return;
  }

  static void ShouldInit(Class<?> klass) {
    if (checkInitialized(klass) == false) {
      System.out.println(klass.getName() + " should be initialized!");
    }
  }

  static void ShouldNotInit(Class<?> klass) {
    if (checkInitialized(klass) == true) {
      System.out.println(klass.getName() + " should not be initialized!");
    }
  }

  public static native boolean checkAppImageLoaded();
  public static native boolean checkAppImageContains(Class<?> klass);
  public static native boolean checkInitialized(Class<?> klass);
}

enum Day {
    SUNDAY, MONDAY, TUESDAY, WEDNESDAY,
    THURSDAY, FRIDAY, SATURDAY
}

class ClInit {

  static String ooo = "OoooooO";
  static String str;
  static int z;
  int x, y;
  public static volatile int a = 100;

  static {
    StringBuilder sb = new StringBuilder();
    sb.append("Hello ");
    sb.append("World!");
    str = sb.toString();
  }

  static {
    z = 0xFF;
    z += 0xFF00;
    z += 0xAA0000;
  }

  {
    for(int i = 0; i < 100; i++) {
      x += i;
    }
  }

  {
    y = this.x;
    for(int i = 0; i < 40; i++) {
      y += i;
    }
  }

  ClInit() {

  }

  int getX() {
    return x;
  }

  int getZ() {
    return z;
  }

  int getY() {
    return y;
  }

  int getA() {
    return a;
  }
}

class A {
  public static int a = 2;
  static {
    a = 5;
  }
}

class D {
  static int d;
  static {
    d = E.e; // fail here
  }
}

class E {
  public static final int e;
  static {
    e = 100;
  }
}

class G {
  static G g;
  static int i;
  static {
    g = new Gs();
    i = A.a;
  }
}

class Gs extends G { }

class QC {
  static Class<?> klazz[] = new Class<?>[]{E.class};
}

class ClinitE {
  static {
    if (Math.sin(3) < 0.5) {
      // throw anyway, can't initialized
      throw new ExceptionInInitializerError("Can't initialize this class!");
    }
  }
}


