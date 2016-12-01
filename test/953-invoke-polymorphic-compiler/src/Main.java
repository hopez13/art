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
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class Main {
  public static void assertTrue(boolean value) {
    if (!value) {
      throw new AssertionError("assertTrue value: " + value);
    }
  }

  public static void assertFalse(boolean value) {
    if (value) {
      throw new AssertionError("assertTrue value: " + value);
    }
  }

  public static void assertEquals(int i1, int i2) {
    if (i1 == i2) { return; }
    throw new AssertionError("assertEquals i1: " + i1 + ", i2: " + i2);
  }

  public static void assertEquals(long i1, long i2) {
    if (i1 == i2) { return; }
    throw new AssertionError("assertEquals l1: " + i1 + ", l2: " + i2);
  }

  public static void assertEquals(Object o, Object p) {
    if (o == p) { return; }
    if (o != null && p != null && o.equals(p)) { return; }
    throw new AssertionError("assertEquals: o1: " + o + ", o2: " + p);
  }

  public static void assertEquals(String s1, String s2) {
    if (s1 == s2) {
      return;
    }

    if (s1 != null && s2 != null && s1.equals(s2)) {
      return;
    }

    throw new AssertionError("assertEquals s1: " + s1 + ", s2: " + s2);
  }

  public static void fail() {
    System.out.println("fail");
    Thread.dumpStack();
  }

  public static void fail(String message) {
    System.out.println("fail: " + message);
    Thread.dumpStack();
  }

  public static int Min2Print2(int a, int b) {
    int[] values = new int[] { a, b };
    System.out.println("Running Main.Min2Print2(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static int Min2Print3(int a, int b, int c) {
    int[] values = new int[] { a, b, c };
    System.out.println("Running Main.Min2Print3(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static int Min2Print6(int a, int b, int c, int d, int e, int f) {
    int[] values = new int[] { a, b, c, d, e, f };
    System.out.println("Running Main.Min2Print6(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static int Min2Print26(int a, int b, int c, int d,
                                int e, int f, int g, int h,
                                int i, int j, int k, int l,
                                int m, int n, int o, int p,
                                int q, int r, int s, int t,
                                int u, int v, int w, int x,
                                int y, int z) {
    int[] values = new int[] { a, b, c, d, e, f, g, h, i, j, k, l, m,
                               n, o, p, q, r, s, t, u, v, w, x, y, z };
    System.out.println("Running Main.Min2Print26(" + Arrays.toString(values) + ")");
    return a > b ? a : b;
  }

  public static void $opt$BasicTest() throws Throwable {
    MethodHandle mh;
    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print2", MethodType.methodType(int.class, int.class, int.class));
    assertEquals((int) mh.invokeExact(33, -4), 33);
    assertEquals((int) mh.invokeExact(-4, 33), 33);

    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print3",
        MethodType.methodType(int.class, int.class, int.class, int.class));
    assertEquals((int) mh.invokeExact(33, -4, 17), 33);
    assertEquals((int) mh.invokeExact(-4, 17, 33), 17);
    assertEquals((int) mh.invokeExact(17, 33, -4), 33);

    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print6",
        MethodType.methodType(
            int.class, int.class, int.class, int.class, int.class, int.class, int.class));
    assertEquals((int) mh.invokeExact(33, -4, 77, 88, 99, 111), 33);
    try {
        // Too few arguments
        assertEquals((int) mh.invokeExact(33, -4, 77, 88), 33);
        fail("No WMTE for too few arguments");
    } catch (WrongMethodTypeException e) {}
    try {
        // Too many arguments
        assertEquals((int) mh.invokeExact(33, -4, 77, 88, 89, 90, 91), 33);
        fail("No WMTE for too many arguments");
    } catch (WrongMethodTypeException e) {}
    assertEquals((int) mh.invokeExact(-4, 77, 88, 99, 111, 33), 77);
    assertEquals((int) mh.invokeExact(77, 88, 99, 111, 33, -4), 88);
    assertEquals((int) mh.invokeExact(88, 99, 111, 33, -4, 77), 99);
    assertEquals((int) mh.invokeExact(99, 111, 33, -4, 77, 88), 111);
    assertEquals((int) mh.invokeExact(111, 33, -4, 77, 88, 99), 111);

    // A preposterous number of arguments.
    mh = MethodHandles.lookup().findStatic(
        Main.class, "Min2Print26",
        MethodType.methodType(
            // Return-type
            int.class,
            // Arguments
            int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class,
            int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class,
            int.class, int.class, int.class, int.class, int.class, int.class, int.class, int.class,
            int.class, int.class));
    assertEquals(1, (int) mh.invokeExact(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25));
    assertEquals(25, (int) mh.invokeExact(25, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24));
    assertEquals(25, (int) mh.invokeExact(24, 25, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23));

    try {
        // Wrong argument type
        mh.invokeExact("a");
        fail("No WMTE for wrong arguments");
    } catch (WrongMethodTypeException wmte) {}

    try {
        // Invoke on null handle.
        MethodHandle mh0 = null;
        mh0.invokeExact("bad");
        fail("No NPE for you");
    } catch (NullPointerException npe) {}

    System.out.println("BasicTest done.");
  }

  public static void main(String[] args) throws Throwable {
    $opt$BasicTest();
  }
}
