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

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Arrays;

public class ChildClass {
  enum Visibility {
    Public('I'),
    Package('F'),
    Protected('J'),
    Private('D');

    Visibility(char shorty) { mShorty = shorty; }
    public char mShorty;

    public static final char HIDDEN_SHORTY = 'C';
  }

  private static final boolean booleanValues[] = new boolean[] { false, true };

  public static void runTest(String libFileName, boolean expectedParentInBoot,
      boolean expectedChildInBoot) throws Exception {
    System.load(libFileName);

    // Check expectations about loading into boot class path.
    isParentInBoot = (ParentClass.class.getClassLoader().getParent() == null);
    if (isParentInBoot != expectedParentInBoot) {
      throw new RuntimeException("Expected ParentClass " +
                                 (expectedParentInBoot ? "" : "not ") + "in boot class path");
    }
    isChildInBoot = (ChildClass.class.getClassLoader().getParent() == null);
    if (isChildInBoot != expectedChildInBoot) {
      throw new RuntimeException("Expected ChildClass " + (expectedChildInBoot ? "" : "not ") +
                                 "in boot class path");
    }

    boolean sameInBoot = (isParentInBoot == isChildInBoot);

    for (Class klass : new Class<?>[] { ParentClass.class, ParentInterface.class }) {
      for (Visibility visibility : Visibility.values()) {
        for (boolean isHidden : booleanValues) {
          boolean expectCanAccess = isHidden ? sameInBoot : true;
          String constructorArgs =
              "" + visibility.mShorty + (isHidden ? Visibility.HIDDEN_SHORTY : "");

          for (boolean isStatic : booleanValues) {
            String baseName =
                visibility.name() + (isStatic ? "Static" : "") + (isHidden ? "Hidden" : "");
            checkField(klass, "field" + baseName, isStatic, visibility, expectCanAccess);
            checkMethod(klass, "method" + baseName, isStatic, visibility, expectCanAccess);
          }
          if (klass.isInterface()) {
            String name = "methodDefault" + visibility.name() + (isHidden ? "Hidden" : "");
            checkMethod(klass, name, /*isStatic*/ false, visibility, expectCanAccess);
          }

          checkConstructor(klass, constructorArgs, visibility, expectCanAccess);
        }
      }
    }

    checkNullaryConstructors(sameInBoot);

    for (boolean isStatic : booleanValues) {
      for (boolean isHidden : booleanValues) {
        boolean expectCanAccess = isHidden ? sameInBoot : true;
        String suffix = (isStatic ? "Static" : "") + (isHidden ? "Hidden" : "");

        for (boolean isSet : booleanValues) {
          checkLinking("LinkField" + (isSet ? "Set" : "Get") + suffix, /*takesParameter*/ isSet,
              expectCanAccess);
        }
        checkLinking("LinkMethod" + suffix, /*takesParameter*/ false, expectCanAccess);
      }
    }
  }

  private static void checkField(Class<?> klass, String name, boolean isStatic,
      Visibility visibility, boolean canAccess) throws Exception {
    boolean isPublic = (visibility == Visibility.Public);

    if (klass.isInterface() && (!isStatic || !isPublic)) {
      return;
    }

    if (Reflection.canDiscoverWithGetDeclaredField(klass, name) != canAccess) {
      throwDiscoveryException(klass, name, true, "getDeclaredField()", canAccess);
    }

    if (Reflection.canDiscoverWithGetDeclaredFields(klass, name) != canAccess) {
      throwDiscoveryException(klass, name, true, "getDeclaredFields()", canAccess);
    }

    if (Reflection.canDiscoverWithGetField(klass, name) != (canAccess && isPublic)) {
      throwDiscoveryException(klass, name, true, "getField()", (canAccess && isPublic));
    }

    if (Reflection.canDiscoverWithGetFields(klass, name) != (canAccess && isPublic)) {
      throwDiscoveryException(klass, name, true, "getFields()", (canAccess && isPublic));
    }

    if (JNI.canDiscoverField(klass, name, isStatic) != canAccess) {
      throwDiscoveryException(klass, name, true, "JNI", canAccess);
    }

    if (canAccess && Reflection.canObserveFieldHiddenAccessFlags(klass, name)) {
      throwModifiersException(klass, name, true);
    }
  }

  private static void checkMethod(Class<?> klass, String name, boolean isStatic,
      Visibility visibility, boolean canAccess) throws Exception {
    boolean isPublic = (visibility == Visibility.Public);
    if (klass.isInterface() && (!isStatic || !isPublic)) {
      return;
    }

    if (Reflection.canDiscoverWithGetDeclaredMethod(klass, name) != canAccess) {
      throwDiscoveryException(klass, name, false, "getDeclaredMethod()", canAccess);
    }

    if (Reflection.canDiscoverWithGetDeclaredMethods(klass, name) != canAccess) {
      throwDiscoveryException(klass, name, false, "getDeclaredMethods()", canAccess);
    }

    if (Reflection.canDiscoverWithGetMethod(klass, name) != (canAccess && isPublic)) {
      throwDiscoveryException(klass, name, false, "getMethod()", (canAccess && isPublic));
    }

    if (Reflection.canDiscoverWithGetMethods(klass, name) != (canAccess && isPublic)) {
      throwDiscoveryException(klass, name, false, "getMethods()", (canAccess && isPublic));
    }

    if (JNI.canDiscoverMethod(klass, name, isStatic) != canAccess) {
      throwDiscoveryException(klass, name, false, "JNI", canAccess);
    }

    if (canAccess && Reflection.canObserveMethodHiddenAccessFlags(klass, name)) {
      throwModifiersException(klass, name, false);
    }
  }

  private static void checkConstructor(Class<?> klass, String argShorty, Visibility visibility,
      boolean canAccess) throws Exception {
    if (klass.isInterface()) {
      return;
    }

    boolean isPublic = (visibility == Visibility.Public);
    String signature = "(" + argShorty + ")V";
    String fullName = "<init>" + signature;
    Class<?> args[] = getTypesFromShorty(argShorty);

    if (Reflection.canDiscoverWithGetDeclaredConstructor(klass, args) != canAccess) {
      throwDiscoveryException(klass, fullName, false, "getDeclaredConstructor()", canAccess);
    }

    if (Reflection.canDiscoverWithGetDeclaredConstructors(klass, args) != canAccess) {
      throwDiscoveryException(klass, fullName, false, "getDeclaredConstructors()", canAccess);
    }

    if (Reflection.canDiscoverWithGetConstructor(klass, args) != (canAccess && isPublic)) {
      throwDiscoveryException(klass, fullName, false, "getConstructor()", (canAccess && isPublic));
    }

    if (Reflection.canDiscoverWithGetConstructors(klass, args) != (canAccess && isPublic)) {
      throwDiscoveryException(klass, fullName, false, "getConstructors()", (canAccess && isPublic));
    }

    if (JNI.canDiscoverConstructor(klass, signature) != canAccess) {
      throwDiscoveryException(klass, fullName, false, "JNI", canAccess);
    }
  }

  private static void checkNullaryConstructors(boolean canAccessHidden) throws Exception {
    if (!Reflection.canUseNewInstance(ClassWithNullaryConstructor.class)) {
      throw new RuntimeException("Could not construct ClassWithNullaryConstructor");
    }

    if (Reflection.canUseNewInstance(ClassWithHiddenNullaryConstructor.class) != canAccessHidden) {
      throw new RuntimeException("Expected to " + (canAccessHidden ? "" : "not ") +
          "be able to construct ClassWithHiddenNullaryConstructor." +
          "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot);
    }
  }

  private static void checkLinking(String className, boolean takesParameter, boolean canAccess)
      throws Exception {
    if (Linking.canAccess(className, takesParameter) != canAccess) {
      throw new RuntimeException("Expected to " + (canAccess ? "" : "not ") +
          "be able to verify " + className + "." +
          "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot);
    }
  }

  private static Class<?>[] getTypesFromShorty(String argShorty) {
    Class<?> args[] = new Class<?>[argShorty.length()];
    for (int i = 0; i < argShorty.length(); ++i) {
      if (argShorty.charAt(i) == 'I') {
        args[i] = Integer.TYPE;
      } else if (argShorty.charAt(i) == 'J') {
        args[i] = Long.TYPE;
      } else if (argShorty.charAt(i) == 'C') {
        args[i] = Character.TYPE;
      } else if (argShorty.charAt(i) == 'F') {
        args[i] = Float.TYPE;
      } else if (argShorty.charAt(i) == 'D') {
        args[i] = Double.TYPE;
      } else {
        throw new RuntimeException("Unknown type " + argShorty.charAt(i));
      }
    }
    return args;
  }

  private static void throwDiscoveryException(Class<?> klass, String name, boolean isField,
      String fn, boolean canAccess) {
    throw new RuntimeException("Expected " + (isField ? "field " : "method ") + klass.getName() +
        "." + name + " to " + (canAccess ? "" : "not ") + "be discoverable with " + fn + ". " +
        "isParentInBoot = " + isParentInBoot + ", " + "isChildInBoot = " + isChildInBoot);
  }

  private static void throwModifiersException(Class<?> klass, String name, boolean isField) {
    throw new RuntimeException("Expected " + (isField ? "field " : "method ") + klass.getName() +
        "." + name + " to not expose hidden modifiers");
  }

  private static boolean isParentInBoot;
  private static boolean isChildInBoot;
}
