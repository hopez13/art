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

package art;

import java.util.ArrayList;
import java.util.Collections;

public class Test906 {
  public static void run() throws Exception {
    Main.bindAgentJNIForClass(Test906.class);
    doTest();
  }

  public static void doTest() throws Exception {
    A a = new A();
    B b = new B();
    B b2 = new B();
    C c = new C();
    A[] aArray = new A[5];
    String s = "Hello World";

    Main.setTag(a, 1);
    Main.setTag(b, 2);
    Main.setTag(b2, 3);
    Main.setTag(aArray, 4);
    Main.setTag(s, 5);
    Main.setTag(B.class, 100);

    int all = iterateThroughHeapCount(0, null, Integer.MAX_VALUE);
    int tagged = iterateThroughHeapCount(HEAP_FILTER_OUT_UNTAGGED, null, Integer.MAX_VALUE);
    int untagged = iterateThroughHeapCount(HEAP_FILTER_OUT_TAGGED, null, Integer.MAX_VALUE);
    int taggedClass = iterateThroughHeapCount(HEAP_FILTER_OUT_CLASS_UNTAGGED, null,
        Integer.MAX_VALUE);
    int untaggedClass = iterateThroughHeapCount(HEAP_FILTER_OUT_CLASS_TAGGED, null,
        Integer.MAX_VALUE);

    if (all != tagged + untagged) {
      throw new IllegalStateException("Instances: " + all + " != " + tagged + " + " + untagged);
    }
    if (all != taggedClass + untaggedClass) {
      throw new IllegalStateException("By class: " + all + " != " + taggedClass + " + " +
          untaggedClass);
    }
    if (tagged != 6) {
      throw new IllegalStateException(tagged + " tagged objects");
    }
    if (taggedClass != 2) {
      throw new IllegalStateException(tagged + " objects with tagged class");
    }
    if (all == tagged) {
      throw new IllegalStateException("All objects tagged");
    }
    if (all == taggedClass) {
      throw new IllegalStateException("All objects have tagged class");
    }

    long classTags[] = new long[100];
    long sizes[] = new long[100];
    long tags[] = new long[100];
    int lengths[] = new int[100];

    int n = iterateThroughHeapData(HEAP_FILTER_OUT_UNTAGGED, null, classTags, sizes, tags, lengths);
    System.out.println(sort(n, classTags, sizes, tags, lengths));

    iterateThroughHeapAdd(HEAP_FILTER_OUT_UNTAGGED, null);
    n = iterateThroughHeapData(HEAP_FILTER_OUT_UNTAGGED, null, classTags, sizes, tags, lengths);
    System.out.println(sort(n, classTags, sizes, tags, lengths));

    System.out.println(iterateThroughHeapString(Main.getTag(s)));
    System.out.println(Main.getTag(s));

    boolean[] zArray = new boolean[] { false, true };
    Main.setTag(zArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(zArray)));
    System.out.println(Main.getTag(zArray));

    byte[] bArray = new byte[] { 1, 2, 3 };
    Main.setTag(bArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(bArray)));
    System.out.println(Main.getTag(bArray));

    char[] cArray = new char[] { 'A', 'Z' };
    Main.setTag(cArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(cArray)));
    System.out.println(Main.getTag(cArray));

    short[] sArray = new short[] { 1, 2, 3 };
    Main.setTag(sArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(sArray)));
    System.out.println(Main.getTag(sArray));

    int[] iArray = new int[] { 1, 2, 3 };
    Main.setTag(iArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(iArray)));
    System.out.println(Main.getTag(iArray));

    float[] fArray = new float[] { 0.0f, 1.0f };
    Main.setTag(fArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(fArray)));
    System.out.println(Main.getTag(fArray));

    long[] lArray = new long[] { 1, 2, 3 };
    Main.setTag(lArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(lArray)));
    System.out.println(Main.getTag(lArray));

    double[] dArray = new double[] { 0.0, 1.0 };
    Main.setTag(dArray, 1);
    System.out.println(iterateThroughHeapPrimitiveArray(Main.getTag(dArray)));
    System.out.println(Main.getTag(dArray));

    // Force GCs to clean up dirt.
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    doTestPrimitiveFieldsClasses();

    doTestPrimitiveFieldsIntegral();

    // Force GCs to clean up dirt.
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();

    doTestPrimitiveFieldsFloat();

    // Force GCs to clean up dirt.
    Runtime.getRuntime().gc();
    Runtime.getRuntime().gc();
  }

  private static void doTestPrimitiveFieldsClasses() {
    Main.setTag(IntObject.class, 10000);
    System.out.println(iterateThroughHeapPrimitiveFields(10000));
    System.out.println(Main.getTag(IntObject.class));
    Main.setTag(IntObject.class, 0);

    Main.setTag(FloatObject.class, 10000);
    System.out.println(iterateThroughHeapPrimitiveFields(10000));
    System.out.println(Main.getTag(FloatObject.class));
    Main.setTag(FloatObject.class, 0);

    Main.setTag(Inf1.class, 10000);
    System.out.println(iterateThroughHeapPrimitiveFields(10000));
    System.out.println(Main.getTag(Inf1.class));
    Main.setTag(Inf1.class, 0);

    Main.setTag(Inf2.class, 10000);
    System.out.println(iterateThroughHeapPrimitiveFields(10000));
    System.out.println(Main.getTag(Inf2.class));
    Main.setTag(Inf2.class, 0);
  }

  private static void doTestPrimitiveFieldsIntegral() {
    IntObject intObject = new IntObject();
    Main.setTag(intObject, 10000);
    System.out.println(iterateThroughHeapPrimitiveFields(10000));
    System.out.println(Main.getTag(intObject));
  }

  private static void doTestPrimitiveFieldsFloat() {
    FloatObject floatObject = new FloatObject();
    Main.setTag(floatObject, 10000);
    System.out.println(iterateThroughHeapPrimitiveFields(10000));
    System.out.println(Main.getTag(floatObject));
  }

  static class A {
  }

  static class B {
  }

  static class C {
  }

  static class HeapElem implements Comparable<HeapElem> {
    long classTag;
    long size;
    long tag;
    int length;

    public int compareTo(HeapElem other) {
      if (tag != other.tag) {
        return Long.compare(tag, other.tag);
      }
      if (classTag != other.classTag) {
        return Long.compare(classTag, other.classTag);
      }
      if (size != other.size) {
        return Long.compare(size, other.size);
      }
      return Integer.compare(length, other.length);
    }

    public String toString() {
      return "{tag=" + tag + ", class-tag=" + classTag + ", size=" +
          (tag >= 100 ? "<class>" : size)  // Class size is dependent on 32-bit vs 64-bit,
                                           // so strip it.
          + ", length=" + length + "}";
    }
  }

  private static ArrayList<HeapElem> sort(int n, long classTags[], long sizes[], long tags[],
      int lengths[]) {
    ArrayList<HeapElem> ret = new ArrayList<HeapElem>(n);
    for (int i = 0; i < n; i++) {
      HeapElem elem = new HeapElem();
      elem.classTag = classTags[i];
      elem.size = sizes[i];
      elem.tag = tags[i];
      elem.length = lengths[i];
      ret.add(elem);
    }
    Collections.sort(ret);
    return ret;
  }

  private static interface Inf1 {
    public final static int A = 1;
  }

  private static interface Inf2 extends Inf1 {
    public final static int B = 1;
  }

  private static class IntObject implements Inf1 {
    byte b = (byte)1;
    char c= 'a';
    short s = (short)2;
    int i = 3;
    long l = 4;
    Object o = new Object();
    static int sI = 5;
  }

  private static class FloatObject extends IntObject implements Inf2 {
    float f = 1.23f;
    double d = 1.23;
    Object p = new Object();
    static int sI = 6;
  }

  private final static int HEAP_FILTER_OUT_TAGGED = 0x4;
  private final static int HEAP_FILTER_OUT_UNTAGGED = 0x8;
  private final static int HEAP_FILTER_OUT_CLASS_TAGGED = 0x10;
  private final static int HEAP_FILTER_OUT_CLASS_UNTAGGED = 0x20;

  private static native int iterateThroughHeapCount(int heapFilter,
      Class<?> klassFilter, int stopAfter);
  private static native int iterateThroughHeapData(int heapFilter,
      Class<?> klassFilter, long classTags[], long sizes[], long tags[], int lengths[]);
  private static native int iterateThroughHeapAdd(int heapFilter,
      Class<?> klassFilter);
  private static native String iterateThroughHeapString(long tag);
  private static native String iterateThroughHeapPrimitiveArray(long tag);
  private static native String iterateThroughHeapPrimitiveFields(long tag);
}
