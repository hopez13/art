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

import junit.framework.Assert;

public class Main {
  public static void main(String[] args) {
    test_Integer_compareUnsigne_no_fold();
    test_Long_compareUnsigned_no_fold();
    test_Integer_compareUnsigned();
    test_Long_compareUnsigned();
    test_Integer_divideUnsigned_no_fold();
    test_Long_divideUnsigned_no_fold();
    test_Integer_divideUnsigned();
    test_Long_divideUnsigned();
    test_Integer_remainderUnsigned_no_fold();
    test_Long_remainderUnsigned_no_fold();
    test_Integer_remainderUnsigned();
    test_Long_remainderUnsigned();
  }

  public static int $noinline$cmp_unsigned(int a, int b) {
    return Integer.compareUnsigned(a, b);
  }

  public static void test_Integer_compareUnsigne_no_fold() {
    Assert.assertEquals($noinline$cmp_unsigned(100, 100), 0);
    Assert.assertEquals($noinline$cmp_unsigned(100, 1), 1);
    Assert.assertEquals($noinline$cmp_unsigned(1, 100), -1);
    Assert.assertEquals($noinline$cmp_unsigned(-2, 2), 1);
    Assert.assertEquals($noinline$cmp_unsigned(2, -2), -1);
    Assert.assertEquals($noinline$cmp_unsigned(Integer.MAX_VALUE, Integer.MIN_VALUE), -1);
    Assert.assertEquals($noinline$cmp_unsigned(Integer.MIN_VALUE, Integer.MAX_VALUE), 1);
    Assert.assertEquals($noinline$cmp_unsigned(Integer.MAX_VALUE, -1), -1);
    Assert.assertEquals($noinline$cmp_unsigned(-1, Integer.MAX_VALUE), 1);
    Assert.assertEquals($noinline$cmp_unsigned(0, 0), 0);
  }

  public static int $noinline$cmp_unsigned(long a, long b) {
    return Long.compareUnsigned(a, b);
  }

  public static void test_Long_compareUnsigned_no_fold() {
    Assert.assertEquals($noinline$cmp_unsigned(100L, 100L), 0);
    Assert.assertEquals($noinline$cmp_unsigned(100L, 1L), 1);
    Assert.assertEquals($noinline$cmp_unsigned(1L, 100L), -1);
    Assert.assertEquals($noinline$cmp_unsigned(-2, 2), 1);
    Assert.assertEquals($noinline$cmp_unsigned(2, -2), -1);
    Assert.assertEquals($noinline$cmp_unsigned(Long.MAX_VALUE, Long.MIN_VALUE), -1);
    Assert.assertEquals($noinline$cmp_unsigned(Long.MIN_VALUE, Long.MAX_VALUE), 1);
    Assert.assertEquals($noinline$cmp_unsigned(Long.MAX_VALUE, -1L), -1);
    Assert.assertEquals($noinline$cmp_unsigned(-1L, Long.MAX_VALUE), 1);
    Assert.assertEquals($noinline$cmp_unsigned(0L, 0L), 0);
  }

  public static void test_Integer_compareUnsigned() {
    Assert.assertEquals(Integer.compareUnsigned(100, 100), 0);
    Assert.assertEquals(Integer.compareUnsigned(100, 1), 1);
    Assert.assertEquals(Integer.compareUnsigned(1, 100), -1);
    Assert.assertEquals(Integer.compareUnsigned(-2, 2), 1);
    Assert.assertEquals(Integer.compareUnsigned(2, -2), -1);
    Assert.assertEquals(Integer.compareUnsigned(Integer.MAX_VALUE, Integer.MIN_VALUE), -1);
    Assert.assertEquals(Integer.compareUnsigned(Integer.MIN_VALUE, Integer.MAX_VALUE), 1);
    Assert.assertEquals(Integer.compareUnsigned(Integer.MAX_VALUE, -1), -1);
    Assert.assertEquals(Integer.compareUnsigned(-1, Integer.MAX_VALUE), 1);
    Assert.assertEquals(Integer.compareUnsigned(0, 0), 0);
  }

  public static void test_Long_compareUnsigned() {
    Assert.assertEquals(Long.compareUnsigned(100L, 100L), 0);
    Assert.assertEquals(Long.compareUnsigned(100L, 1L), 1);
    Assert.assertEquals(Long.compareUnsigned(1L, 100L), -1);
    Assert.assertEquals(Long.compareUnsigned(-2, 2), 1);
    Assert.assertEquals(Long.compareUnsigned(2, -2), -1);
    Assert.assertEquals(Long.compareUnsigned(Long.MAX_VALUE, Long.MIN_VALUE), -1);
    Assert.assertEquals(Long.compareUnsigned(Long.MIN_VALUE, Long.MAX_VALUE), 1);
    Assert.assertEquals(Long.compareUnsigned(Long.MAX_VALUE, -1L), -1);
    Assert.assertEquals(Long.compareUnsigned(-1L, Long.MAX_VALUE), 1);
    Assert.assertEquals(Long.compareUnsigned(0L, 0L), 0);
  }

  public static int $noinline$divide_unsigned(int a, int b) {
    return Integer.divideUnsigned(a, b);
  }

  public static void test_Integer_divideUnsigned_no_fold() {
    Assert.assertEquals($noinline$divide_unsigned(100, 10), 10);
    Assert.assertEquals($noinline$divide_unsigned(100, 1), 100);
    Assert.assertEquals($noinline$divide_unsigned(1024, 128), 8);
    Assert.assertEquals($noinline$divide_unsigned(12345678, 264), 46763);
    Assert.assertEquals($noinline$divide_unsigned(13, 5), 2);
    Assert.assertEquals($noinline$divide_unsigned(-2, 2), Integer.MAX_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(-1, 2), Integer.MAX_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(100000, -1), 0);
    Assert.assertEquals($noinline$divide_unsigned(Integer.MAX_VALUE, -1), 0);
    Assert.assertEquals($noinline$divide_unsigned(-2, -1), 0);
    Assert.assertEquals($noinline$divide_unsigned(-1, -2), 1);
    Assert.assertEquals($noinline$divide_unsigned(-173448, 13), 330368757);
    Assert.assertEquals($noinline$divide_unsigned(Integer.MIN_VALUE, 2), (1 << 30));
    Assert.assertEquals($noinline$divide_unsigned(-1, Integer.MIN_VALUE), 1);
    Assert.assertEquals($noinline$divide_unsigned(Integer.MAX_VALUE, Integer.MIN_VALUE), 0);
    Assert.assertEquals($noinline$divide_unsigned(Integer.MIN_VALUE, Integer.MAX_VALUE), 1);

    try {
      $noinline$divide_unsigned(1, 0);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static long $noinline$divide_unsigned(long a, long b) {
    return Long.divideUnsigned(a, b);
  }

  private static final long BIG_LONG_VALUE = 739287620162442240L;

  public static void test_Long_divideUnsigned_no_fold() {
    Assert.assertEquals($noinline$divide_unsigned(100L, 10L), 10L);
    Assert.assertEquals($noinline$divide_unsigned(100L, 1L), 100L);
    Assert.assertEquals($noinline$divide_unsigned(1024L, 128L), 8L);
    Assert.assertEquals($noinline$divide_unsigned(12345678L, 264L), 46763L);
    Assert.assertEquals($noinline$divide_unsigned(13L, 5L), 2L);
    Assert.assertEquals($noinline$divide_unsigned(-2L, 2L), Long.MAX_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(-1L, 2L), Long.MAX_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(100000L, -1L), 0L);
    Assert.assertEquals($noinline$divide_unsigned(Long.MAX_VALUE, -1L), 0L);
    Assert.assertEquals($noinline$divide_unsigned(-2L, -1L), 0L);
    Assert.assertEquals($noinline$divide_unsigned(-1L, -2L), 1L);
    Assert.assertEquals($noinline$divide_unsigned(-173448L, 13L), 1418980313362259859L);
    Assert.assertEquals($noinline$divide_unsigned(Long.MIN_VALUE, 2L), (1L << 62));
    Assert.assertEquals($noinline$divide_unsigned(-1L, Long.MIN_VALUE), 1L);
    Assert.assertEquals($noinline$divide_unsigned(Long.MAX_VALUE, Long.MIN_VALUE), 0L);
    Assert.assertEquals($noinline$divide_unsigned(Long.MIN_VALUE, Long.MAX_VALUE), 1L);
    Assert.assertEquals($noinline$divide_unsigned(Long.MAX_VALUE, 1L), Long.MAX_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(Long.MIN_VALUE, 1L), Long.MIN_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(BIG_LONG_VALUE, BIG_LONG_VALUE), 1L);
    Assert.assertEquals($noinline$divide_unsigned(BIG_LONG_VALUE, 1L), BIG_LONG_VALUE);
    Assert.assertEquals($noinline$divide_unsigned(BIG_LONG_VALUE, 1024L), 721960566564885L);
    Assert.assertEquals($noinline$divide_unsigned(BIG_LONG_VALUE, 0x1FFFFFFFFL), 86064406L);

    try {
      $noinline$divide_unsigned(1L, 0L);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static void test_Integer_divideUnsigned() {
    Assert.assertEquals(Integer.divideUnsigned(100, 10), 10);
    Assert.assertEquals(Integer.divideUnsigned(100, 1), 100);
    Assert.assertEquals(Integer.divideUnsigned(1024, 128), 8);
    Assert.assertEquals(Integer.divideUnsigned(12345678, 264), 46763);
    Assert.assertEquals(Integer.divideUnsigned(13, 5), 2);
    Assert.assertEquals(Integer.divideUnsigned(-2, 2), Integer.MAX_VALUE);
    Assert.assertEquals(Integer.divideUnsigned(-1, 2), Integer.MAX_VALUE);
    Assert.assertEquals(Integer.divideUnsigned(100000, -1), 0);
    Assert.assertEquals(Integer.divideUnsigned(Integer.MAX_VALUE, -1), 0);
    Assert.assertEquals(Integer.divideUnsigned(-2, -1), 0);
    Assert.assertEquals(Integer.divideUnsigned(-1, -2), 1);
    Assert.assertEquals(Integer.divideUnsigned(-173448, 13), 330368757);
    Assert.assertEquals(Integer.divideUnsigned(Integer.MIN_VALUE, 2), (1 << 30));
    Assert.assertEquals(Integer.divideUnsigned(-1, Integer.MIN_VALUE), 1);
    Assert.assertEquals(Integer.divideUnsigned(Integer.MAX_VALUE, Integer.MIN_VALUE), 0);
    Assert.assertEquals(Integer.divideUnsigned(Integer.MIN_VALUE, Integer.MAX_VALUE), 1);

    try {
      Integer.divideUnsigned(1, 0);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static void test_Long_divideUnsigned() {
    Assert.assertEquals(Long.divideUnsigned(100L, 10L), 10L);
    Assert.assertEquals(Long.divideUnsigned(100L, 1L), 100L);
    Assert.assertEquals(Long.divideUnsigned(1024L, 128L), 8L);
    Assert.assertEquals(Long.divideUnsigned(12345678L, 264L), 46763L);
    Assert.assertEquals(Long.divideUnsigned(13L, 5L), 2L);
    Assert.assertEquals(Long.divideUnsigned(-2L, 2L), Long.MAX_VALUE);
    Assert.assertEquals(Long.divideUnsigned(-1L, 2L), Long.MAX_VALUE);
    Assert.assertEquals(Long.divideUnsigned(100000L, -1L), 0L);
    Assert.assertEquals(Long.divideUnsigned(Long.MAX_VALUE, -1L), 0L);
    Assert.assertEquals(Long.divideUnsigned(-2L, -1L), 0L);
    Assert.assertEquals(Long.divideUnsigned(-1L, -2L), 1L);
    Assert.assertEquals(Long.divideUnsigned(-173448L, 13L), 1418980313362259859L);
    Assert.assertEquals(Long.divideUnsigned(Long.MIN_VALUE, 2L), (1L << 62));
    Assert.assertEquals(Long.divideUnsigned(-1L, Long.MIN_VALUE), 1L);
    Assert.assertEquals(Long.divideUnsigned(Long.MAX_VALUE, Long.MIN_VALUE), 0L);
    Assert.assertEquals(Long.divideUnsigned(Long.MIN_VALUE, Long.MAX_VALUE), 1L);
    Assert.assertEquals(Long.divideUnsigned(Long.MAX_VALUE, 1L), Long.MAX_VALUE);
    Assert.assertEquals(Long.divideUnsigned(Long.MIN_VALUE, 1L), Long.MIN_VALUE);
    Assert.assertEquals(Long.divideUnsigned(BIG_LONG_VALUE, BIG_LONG_VALUE), 1L);
    Assert.assertEquals(Long.divideUnsigned(BIG_LONG_VALUE, 1L), BIG_LONG_VALUE);
    Assert.assertEquals(Long.divideUnsigned(BIG_LONG_VALUE, 1024L), 721960566564885L);
    Assert.assertEquals(Long.divideUnsigned(BIG_LONG_VALUE, 0x1FFFFFFFFL), 86064406L);

    try {
      Long.divideUnsigned(1L, 0L);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static int $noinline$remainder_unsigned(int a, int b) {
    return Integer.remainderUnsigned(a, b);
  }

  public static void test_Integer_remainderUnsigned_no_fold() {
    Assert.assertEquals($noinline$remainder_unsigned(100, 10), 0);
    Assert.assertEquals($noinline$remainder_unsigned(100, 1), 0);
    Assert.assertEquals($noinline$remainder_unsigned(1024, 127), 8);
    Assert.assertEquals($noinline$remainder_unsigned(12345678, 264), 246);
    Assert.assertEquals($noinline$remainder_unsigned(13, 5), 3);
    Assert.assertEquals($noinline$remainder_unsigned(-2, 2), 0);
    Assert.assertEquals($noinline$remainder_unsigned(-1, 2), 1);
    Assert.assertEquals($noinline$remainder_unsigned(100000, -1), 100000);
    Assert.assertEquals($noinline$remainder_unsigned(Integer.MAX_VALUE, -1), Integer.MAX_VALUE);
    Assert.assertEquals($noinline$remainder_unsigned(-2, -1), -2);
    Assert.assertEquals($noinline$remainder_unsigned(-1, -2), 1);
    Assert.assertEquals($noinline$remainder_unsigned(-173448, 13), 7);
    Assert.assertEquals($noinline$remainder_unsigned(Integer.MIN_VALUE, 2), 0);
    Assert.assertEquals($noinline$remainder_unsigned(-1, Integer.MIN_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals($noinline$remainder_unsigned(Integer.MAX_VALUE, Integer.MIN_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals($noinline$remainder_unsigned(Integer.MIN_VALUE, Integer.MAX_VALUE), 1);

    try {
      $noinline$remainder_unsigned(1, 0);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static long $noinline$remainder_unsigned(long a, long b) {
    return Long.remainderUnsigned(a, b);
  }

  public static void test_Long_remainderUnsigned_no_fold() {
    Assert.assertEquals($noinline$remainder_unsigned(100L, 10L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(100L, 1L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(1024L, 127L), 8L);
    Assert.assertEquals($noinline$remainder_unsigned(12345678L, 264L), 246L);
    Assert.assertEquals($noinline$remainder_unsigned(13L, 5L), 3L);
    Assert.assertEquals($noinline$remainder_unsigned(-2L, 2L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(-1L, 2L), 1L);
    Assert.assertEquals($noinline$remainder_unsigned(100000L, -1L), 100000L);
    Assert.assertEquals($noinline$remainder_unsigned(Long.MAX_VALUE, -1L), Long.MAX_VALUE);
    Assert.assertEquals($noinline$remainder_unsigned(-2L, -1L), -2L);
    Assert.assertEquals($noinline$remainder_unsigned(-1L, -2L), 1L);
    Assert.assertEquals($noinline$remainder_unsigned(-173448L, 13L), 1L);
    Assert.assertEquals($noinline$remainder_unsigned(Long.MIN_VALUE, 2L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(-1L, Long.MIN_VALUE), Long.MAX_VALUE);
    Assert.assertEquals($noinline$remainder_unsigned(Long.MAX_VALUE, Long.MIN_VALUE), Long.MAX_VALUE);
    Assert.assertEquals($noinline$remainder_unsigned(Long.MIN_VALUE, Long.MAX_VALUE), 1L);
    Assert.assertEquals($noinline$remainder_unsigned(Long.MAX_VALUE, 1L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(Long.MIN_VALUE, 1L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(BIG_LONG_VALUE, BIG_LONG_VALUE), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(BIG_LONG_VALUE, 1L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(BIG_LONG_VALUE, 1024L), 0L);
    Assert.assertEquals($noinline$remainder_unsigned(BIG_LONG_VALUE, 0x1FFFFFFFFL), 2009174294L);
    Assert.assertEquals($noinline$remainder_unsigned(BIG_LONG_VALUE, -38766615688777L), 739287620162442240L);

    try {
      $noinline$remainder_unsigned(1L, 0L);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static void test_Integer_remainderUnsigned() {
    Assert.assertEquals(Integer.remainderUnsigned(100, 10), 0);
    Assert.assertEquals(Integer.remainderUnsigned(100, 1), 0);
    Assert.assertEquals(Integer.remainderUnsigned(1024, 127), 8);
    Assert.assertEquals(Integer.remainderUnsigned(12345678, 264), 246);
    Assert.assertEquals(Integer.remainderUnsigned(13, 5), 3);
    Assert.assertEquals(Integer.remainderUnsigned(-2, 2), 0);
    Assert.assertEquals(Integer.remainderUnsigned(-1, 2), 1);
    Assert.assertEquals(Integer.remainderUnsigned(100000, -1), 100000);
    Assert.assertEquals(Integer.remainderUnsigned(Integer.MAX_VALUE, -1), Integer.MAX_VALUE);
    Assert.assertEquals(Integer.remainderUnsigned(-2, -1), -2);
    Assert.assertEquals(Integer.remainderUnsigned(-1, -2), 1);
    Assert.assertEquals(Integer.remainderUnsigned(-173448, 13), 7);
    Assert.assertEquals(Integer.remainderUnsigned(Integer.MIN_VALUE, 2), 0);
    Assert.assertEquals(Integer.remainderUnsigned(-1, Integer.MIN_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(Integer.remainderUnsigned(Integer.MAX_VALUE, Integer.MIN_VALUE), Integer.MAX_VALUE);
    Assert.assertEquals(Integer.remainderUnsigned(Integer.MIN_VALUE, Integer.MAX_VALUE), 1);

    try {
      Integer.remainderUnsigned(1, 0);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }

  public static void test_Long_remainderUnsigned() {
    Assert.assertEquals(Long.remainderUnsigned(100L, 10L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(100L, 1L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(1024L, 127L), 8L);
    Assert.assertEquals(Long.remainderUnsigned(12345678L, 264L), 246L);
    Assert.assertEquals(Long.remainderUnsigned(13L, 5L), 3L);
    Assert.assertEquals(Long.remainderUnsigned(-2L, 2L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(-1L, 2L), 1L);
    Assert.assertEquals(Long.remainderUnsigned(100000L, -1L), 100000L);
    Assert.assertEquals(Long.remainderUnsigned(Long.MAX_VALUE, -1L), Long.MAX_VALUE);
    Assert.assertEquals(Long.remainderUnsigned(-2L, -1L), -2L);
    Assert.assertEquals(Long.remainderUnsigned(-1L, -2L), 1L);
    Assert.assertEquals(Long.remainderUnsigned(-173448L, 13L), 1L);
    Assert.assertEquals(Long.remainderUnsigned(Long.MIN_VALUE, 2L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(-1L, Long.MIN_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(Long.remainderUnsigned(Long.MAX_VALUE, Long.MIN_VALUE), Long.MAX_VALUE);
    Assert.assertEquals(Long.remainderUnsigned(Long.MIN_VALUE, Long.MAX_VALUE), 1L);
    Assert.assertEquals(Long.remainderUnsigned(Long.MAX_VALUE, 1L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(Long.MIN_VALUE, 1L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(BIG_LONG_VALUE, BIG_LONG_VALUE), 0L);
    Assert.assertEquals(Long.remainderUnsigned(BIG_LONG_VALUE, 1L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(BIG_LONG_VALUE, 1024L), 0L);
    Assert.assertEquals(Long.remainderUnsigned(BIG_LONG_VALUE, 0x1FFFFFFFFL), 2009174294L);
    Assert.assertEquals(Long.remainderUnsigned(BIG_LONG_VALUE, -38766615688777L), 739287620162442240L);

    try {
      Long.remainderUnsigned(1L, 0L);
      Assert.fail("Unreachable");
    } catch (ArithmeticException expected) {
    }
  }
}
