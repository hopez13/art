/*
 * Copyright (C) 2018 The Android Open Source Project
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

// Simple tests for partial loop unroll with unknown iterations
public class Main {
  int id;
  public Main() {
  }

  // this loop will be unrolled
  private static int testArraySum(int[] x) {
    int result = 0;
    for (int i = 0; i < x.length; i++) {
      result += x[i];
    }
    return result;
  }

  // this loop will be unrolled
  private static int testArraySumDown(int[] x) {
    int result = 0;
    for (int i = x.length - 1; i >= 0; i--) {
      result += x[i];
    }
    return result;
  }

  // this loop will be unrolled
  private static int testArraySumWhile(int[] x) {
    int i = 0;
    int result = 0;
    while (i < x.length) {
      result += x[i++];
    }
    return result;
  }

  // this loop won't be unrolled
  // Current implementation requires that back-edge input of PHI (inside header) 
  // should contain PHI as one of it's input.
  // In below loop, PHI for "result" variable violate above condition.
  private static int testArraySumByTwo(int x[]) {
    int n = x.length / 2;
    int result = 0;
    for (int i = 0; i < n; i++) {
      int ii = i << 1;
      result += x[ii];
      result += x[ii + 1];
    }
    return result;
  }

  // this loop will be unrolled
  private static void testArrayInitialize(int[] array) {
    int n = array.length;
    for (int i = 0; i < n; i++) {
       array[i] = (n % 2) + i;
    }
  }

  // this loop won't be unrolled
  // Current implementation requires that back-edge input of PHI (inside header) 
  // should contain PHI as one of it's input.
  // In below loop, PHI for "hash" variable violate above condition.
  private static int testHashCode(int[] x) {
    int hash = 0;
    for (int i = 0; i < x.length; i++) {
       hash = 31 * hash + x[i];
    }
    return hash;
  }

  // this loop will be unrolled
  private static int[] testArrayCopy(int x[]) {
    int n = x.length;
    int y[] = new int[n];
    for (int i = 0; i < n; i++) {
      y[i] = x[i];
    }
    return y;
  }

  // this loop will be unrolled
  private static void testFloatInitialize(float[] array) {
    int n = array.length;
    array[0] = 99;
    for (int i = 1; i < n; i++) {
      array[i] = array[i-1] + i;
    }
  }

  // this loop won't be unrolled
  // Current implementation requires that number of instructions inside loop should
  // be less that LSD size after unrolling.
  // But, number of instruction after unrolling exceeds the LSD size
  private static float testFloatCode(float[] x) {
    int n = x.length;
    float result=0, result1=1, result2=2, result3=3 ;

    for (int i = 0; i < n; ) {
      result += x[i++];
      result1 += x[i++] + 1.5;
      result2 += x[i++] + 2.5;
      result3 += x[i++] + 3.5;
    }

    return result + result1 + result2 + result3;
  }

  // this loop will be unrolled
  public double[] testRandomVector(int N) {
     double A[] = new double[N];
 
     for (int i=0; i<N; i++)
        A[i] = (double) (N*N) / (i+1);
     return A;
  }

  // this loop will be unrolled
 public static int periodicIdiom(int tc) {
    int[] x = { 1, 3 };
    // Loop with periodic sequence (0, 1).
    int k = 0;
    int result = 0;
    for (int i = 0; i < tc; i++) {
      result += x[k];
      k = 1 - k;
    }
    return result;
  }

  // this loop won't be unrolled
  // Current implementation requires that back-edge input of PHI (inside header) 
  // should contain PHI as one of it's input.
  // In below loop, PHI for "sum" & "xor" variable violate above condition.
  public int testCheckSum(int[] arr) {
     byte sum = 0;
     byte xor = 0;
     for (int i = 0; i < arr.length; i++) {
          sum += arr[i];
          xor |= arr[i];
     }
        return (xor << 8) + sum;
    }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(double expected, double result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public void run(int N) {
    int[] iarray = new int[N];
    //double[] darray = new double[N];

    // run the benchmark
    testArrayInitialize(iarray);
    expectEquals(4950, testArraySum(iarray));
    expectEquals(4950, testArraySumDown(iarray));
    expectEquals(4950, testArraySumWhile(iarray));
    expectEquals(58926130, testHashCode(iarray));

    int[] rr = testArrayCopy(iarray);
    expectEquals(4950, testArraySumByTwo(rr));

    expectEquals(32598, testCheckSum(rr));

    float[] farray = new float[N];
    testFloatInitialize(farray);
    expectEquals(176743.5, testFloatCode(farray));
  }

    /** Simple Test routine. */
  public static void main(String args[]){
     Main plu = new Main();
     plu.run(100);
     System.out.println("passed");
  }
}
