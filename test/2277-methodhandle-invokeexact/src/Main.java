/*
 * Copyright (C) 2024 The Android Open Source Project
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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.util.Optional;

public class Main {

    public static void main(String[] args) throws Throwable {
      testNoArgsCalls();
      testMethodHandleFromOtherDex();
      Multi.$noinline$testMHFromMain(OPTIONAL_GET);
    }

    private static void testMethodHandleFromOtherDex() throws Throwable {
      MethodHandle mh = Multi.$noinline$getMethodHandle();
      Optional<String> nonEmpty = Optional.<String>of("hello");
      Object returnedObject = mh.invokeExact(nonEmpty);
      System.out.println("Multi.$noinline$getMethodHandle().invokeExact(nonEmpty)=" + returnedObject);

      try {
        mh.invokeExact(nonEmpty);
        fail("mh.type() is (Optional)Object, but callsite is (Optional)V");
      } catch (WrongMethodTypeException expected) {}
    }

    private static void testNoArgsCalls() throws Throwable {
      VOID_METHOD.invokeExact(new A());
      int returnedInt = (int) RETURN_INT.invokeExact(new A());
      System.out.println("A.returnInt()=" + returnedInt);
      double returnedDouble = (double) RETURN_DOUBLE.invokeExact(new A());
      System.out.println("A.returnDouble()=" + returnedDouble);

      try {
        INTERFACE_DEFAULT_METHOD.invokeExact(new A());
        fail("Callsite type is (Main$A)V, but MethodHandle is (Main$I)V");
      } catch (WrongMethodTypeException ignored) {}

      INTERFACE_DEFAULT_METHOD.invokeExact((I) new A());
      OVERWRITTEN_INTERFACE_DEFAULT_METHOD.invokeExact((I) new A());

      int privateIntA = (int) A_PRIVATE_RETURN_INT.invokeExact(new A());
      System.out.println("A.privateReturnInt()=" + privateIntA);

      int privateIntB = (int) B_PRIVATE_RETURN_INT.invokeExact(new B());
      System.out.println("B.privateReturnInt()=" + privateIntB);
      privateIntB = (int) B_PRIVATE_RETURN_INT.invokeExact((B) new A());
      System.out.println("B_PRIVATE_RETURN_INT(new A())=" + privateIntB);

      try {
        EXCEPTION_THROWING_METHOD.invokeExact(new A());
        fail("target method always throws");
      } catch (RuntimeException e) {
        System.out.println(e.getMessage());
      }

      try {
        RETURN_INT.invokeExact(new A());
        fail("Callsite type is (Main$A)V, but MethodHandle is (Main$A)I");
      } catch (WrongMethodTypeException ignored) {}

      String returnedString = (String) STATIC_METHOD.invokeExact(new A());
      System.out.println("A.staticMethod()=" + returnedString);
    }

    private static void fail(String msg) {
      throw new AssertionError(msg);
    }

    private static final MethodHandle VOID_METHOD;
    private static final MethodHandle RETURN_DOUBLE;
    private static final MethodHandle RETURN_INT;
    private static final MethodHandle B_PRIVATE_RETURN_INT;
    private static final MethodHandle A_PRIVATE_RETURN_INT;
    private static final MethodHandle STATIC_METHOD;
    private static final MethodHandle EXCEPTION_THROWING_METHOD;
    private static final MethodHandle INTERFACE_DEFAULT_METHOD;
    private static final MethodHandle OVERWRITTEN_INTERFACE_DEFAULT_METHOD;
    private static final MethodHandle OPTIONAL_GET;

    static {
        try {
            VOID_METHOD = MethodHandles.lookup()
                      .findVirtual(A.class, "voidMethod", MethodType.methodType(void.class));
            RETURN_DOUBLE = MethodHandles.lookup()
                      .findVirtual(A.class, "returnDouble", MethodType.methodType(double.class));
            RETURN_INT = MethodHandles.lookup()
                      .findVirtual(A.class, "returnInt", MethodType.methodType(int.class));
            A_PRIVATE_RETURN_INT = MethodHandles.privateLookupIn(A.class, MethodHandles.lookup())
                      .findVirtual(A.class, "privateReturnInt", MethodType.methodType(int.class));
            B_PRIVATE_RETURN_INT = MethodHandles.privateLookupIn(B.class, MethodHandles.lookup())
                      .findVirtual(B.class, "privateReturnInt", MethodType.methodType(int.class));
            STATIC_METHOD = MethodHandles.lookup()
                      .findStatic(
                          A.class, "staticMethod", MethodType.methodType(String.class, A.class));
            EXCEPTION_THROWING_METHOD = MethodHandles.lookup()
                      .findVirtual(A.class, "throwException", MethodType.methodType(void.class));
            INTERFACE_DEFAULT_METHOD = MethodHandles.lookup()
                      .findVirtual(I.class, "defaultMethod", MethodType.methodType(void.class));
            OVERWRITTEN_INTERFACE_DEFAULT_METHOD = MethodHandles.lookup()
                      .findVirtual(I.class, "overrideMe", MethodType.methodType(void.class));
            OPTIONAL_GET = MethodHandles.lookup()
                      .findVirtual(Optional.class, "get", MethodType.methodType(Object.class));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    static interface I {
      public default void defaultMethod() {
        System.out.println("in I.defaultMethod");
      }

      public default void overrideMe() {
        throw new RuntimeException("should be overwritten");
      }
    }

    static class A extends B implements I {
        public int field;
        public void voidMethod() {
          System.out.println("in voidMethod");
        }

        public void throwException() {
          throw new RuntimeException("I am from throwException");
        }

        @Override
        public void overrideMe() {
          System.out.println("in A.overrideMe");
        }

        public double returnDouble() {
          return 42.0d;
        }

        public int returnInt() {
          return 42;
        }

        private int privateReturnInt() {
          return 1042;
        }

        public static String staticMethod(A a) {
          return "staticMethod";
        }

        public static double staticMethod() {
          return 41.0d;
        }
    }

    static class B {
      private int privateReturnInt() {
        return 9999;
      }
    }
}
