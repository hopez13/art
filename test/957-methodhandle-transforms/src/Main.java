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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.invoke.Transformers.Transformer;

import dalvik.system.EmulatedStackFrame;

public class Main {

  public static void specialFunction(boolean z, char a, short b, int c, long d,
                                     float e, double f, String g, Object h) {
    System.out.println("boolean: " + z);
    System.out.println("char: " + a);
    System.out.println("short: " + b);
    System.out.println("int: " + c);
    System.out.println("long: " + d);
    System.out.println("float: " + e);
    System.out.println("double: " + f);
    System.out.println("String: " + g);
    System.out.println("Object: " + h);
  }

  public static class DelegatingTransformer extends Transformer {
    private final MethodHandle delegate;

    public DelegatingTransformer(MethodHandle delegate) {
      super(delegate.type());
      this.delegate = delegate;
    }

    @Override
    public void transform(EmulatedStackFrame stackFrame) throws Throwable {
      Object[] references = stackFrame.getReferences();
      System.out.println("references.length : " + references.length);
      System.out.println("references[0] : " + references[0]);
      System.out.println("references[1] : " + references[1]);
      delegate.invoke(stackFrame);
    }
  }

  public static void main(String[] args) throws Throwable {
    testThrowException();
    testDelegation();
    test_MethodHandles_DropArguments();
    test_MethodHandles_catchException();
    test_MethodHandles_guardWithTest();
  }

  public static void testDelegation() throws Throwable {
    MethodHandle specialFunctionHandle = MethodHandles.lookup().findStatic(
        Main.class, "specialFunction", MethodType.methodType(void.class,
          new Class<?>[] { boolean.class, char.class, short.class, int.class, long.class,
            float.class, double.class, String.class, Object.class }));

    DelegatingTransformer delegate = new DelegatingTransformer(specialFunctionHandle);
    delegate.invoke(false, 'h', (short) 56, 72, Integer.MAX_VALUE + 42l, 0.56f, 100.0d, "hello", "goodbye");
  }

  public static void testThrowException() throws Throwable {
    MethodHandle handle = MethodHandles.throwException(String.class,
        IllegalArgumentException.class);

    if (handle.type().returnType() != String.class) {
      System.out.println("Unexpected return type for handle: " + handle +
          " [ " + handle.type() + "]");
    }

    try {
      handle.invoke();
      System.out.println("Expected an exception of type: java.lang.IllegalArgumentException");
    } catch (IllegalArgumentException expected) {
    }
  }

  public static void test_MethodHandles_DropArguments() throws Throwable {
    MethodHandle stringConcat = MethodHandles.lookup().findVirtual(String.class,
          "concat", MethodType.methodType(String.class, String.class));

    System.out.println("--- DropArguments");

    // Create a handle that drops two arguments at position 0, one of type int and another
    // of type long.
    MethodHandle dropped = MethodHandles.dropArguments(stringConcat, 0, int.class, long.class);

    // Should be equivalent to "y".concat("z");
    System.out.println((String) dropped.invokeExact(123, 456l, "y", "z"));

    // Same as the above, except that we first do an argument conversion of 456 to 456l
    // since this is a non exact invoke.
    System.out.println((String) dropped.invoke(123, 456, "y", "z"));

    // Same as the above, except we're dropping arguments at position 2.
    dropped = MethodHandles.dropArguments(stringConcat, 2, int.class, long.class);
    System.out.println((String) dropped.invokeExact("y", "z", 123, 456l));
    System.out.println((String) dropped.invoke("y", "z",  123, 456));

    // Same as the above, except we're dropping arguments at position 1.
    dropped = MethodHandles.dropArguments(stringConcat, 1, int.class, long.class);
    System.out.println((String) dropped.invokeExact("y", 123, 456l, "z"));
    System.out.println((String) dropped.invoke("y", 123, 456, "z"));
  }


  public static void catchException_target(String foo, String bar) {
    throw new IllegalStateException(bar);
  }

  public static void catchException_handler(IllegalStateException ex, String foo) {
    System.out.println("Exception message :" + ex.getMessage());
    System.out.println("Second argument :" + foo);
  }

  public static void test_MethodHandles_catchException() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();

    MethodHandle target = lookup.findStatic(Main.class, "catchException_target",
        MethodType.methodType(void.class, new Class<?>[] { String.class, String.class }));
    MethodHandle handler = lookup.findStatic(Main.class, "catchException_handler",
        MethodType.methodType(void.class, new Class<?>[] { IllegalStateException.class, String.class }));

    MethodHandle adapter = MethodHandles.catchException(target, IllegalStateException.class, handler);

    adapter.invoke("foo", "bar");
  }

  public static boolean guardWithTest_test(String test) {
    System.out.println("Test: " + test);
    return "true".equals(test);
  }

  public static void guardWithTest_target(String a, String b) {
    System.out.println("target: " + a + ", " + b);
  }

  public static void guardWithTest_fallback(String a, String b) {
    System.out.println("fallback: " + a + ", " + b);
  }

  public static void test_MethodHandles_guardWithTest() throws Throwable {
    MethodHandles.Lookup lookup = MethodHandles.lookup();

    MethodHandle test = lookup.findStatic(Main.class, "guardWithTest_test",
        MethodType.methodType(boolean.class, String.class));
    MethodHandle target = lookup.findStatic(Main.class, "guardWithTest_target",
        MethodType.methodType(void.class, new Class<?>[] { String.class, String.class }));
    MethodHandle fallback = lookup.findStatic(Main.class, "guardWithTest_fallback",
        MethodType.methodType(void.class, new Class<?>[] { String.class, String.class }));

    MethodHandle adapter = MethodHandles.guardWithTest(test, target, fallback);

    adapter.invoke("true", "blue");
    adapter.invoke("false", "bloss");
  }
}


