/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.calculator2.evaluation;

import java.util.Random;

public class UnifiedRealVsFpTest {

  private static final int WRONG = 3;
  private static final UnifiedReal THREE = UnifiedReal.valueOf(3);
  private static final UnifiedReal MINUS17 = UnifiedReal.valueOf(-17);
  private static Random rand;
  private static final int COMPARE_PREC = -2000;

  private static void checkComparable(UnifiedReal x, UnifiedReal y) {
    if (!x.isComparable(y)) {
      throw new AssertionError(x.toNiceString() + " not comparable to " + y.toNiceString());
    }
  }

  /**
   * Return the difference between fpVal and urVal in ulps. 0 ==> correctly rounded. fpVal is a/the
   * closest representable value fp value. 1 ==> within 1 ulp. fpVal is either the next higher or
   * next lower fp value. If the exact answer is representable, then fpVal is exactly urVal, and
   * hence we would have returned 0, not 1. 2 ==> within 2 ulps. fpVal is one removed from the next
   * higher or lower fpVal. WRONG ==> More than 2 ulps error. We optimistically assume that either
   * urVal is known to be rational, or urVal is irrational, and thus all of our comparisons will
   * converge. In a few cases below, we explicitly avoid empirically observed divergence resulting
   * from violation of this assumption.
   */
  private static int ulpError(double fpVal, UnifiedReal urVal) {
    assertTrue("Property wrong for " + urVal.toNiceString(),
        urVal.propertyCorrect(COMPARE_PREC));  // Check UnifiedReal for internal consistency.
    final UnifiedReal fpAsUr = UnifiedReal.valueOf(fpVal);
    checkComparable(fpAsUr, urVal);
    final int errorSign = fpAsUr.compareTo(urVal);
    if (errorSign == 0) {
      return 0; // Exactly equal.
    }
    if (errorSign < 0) {
      return ulpError(-fpVal, urVal.negate());
    }
    // errorSign > 0
    final double prevFp = Math.nextAfter(fpVal, Double.NEGATIVE_INFINITY);
    if (Double.isInfinite(prevFp)) {
      // Most negative representable value was returned. True result is smaller.
      // That seems to qualify as "correctly rounded".
      return 0;
    }
    final UnifiedReal prev = UnifiedReal.valueOf(prevFp);
    checkComparable(prev, urVal);
    if (prev.compareTo(urVal) >= 0) {
      // prev is a better approximation.
      final double prevprevFp = Math.nextAfter(prevFp, Double.NEGATIVE_INFINITY);
      if (Double.isInfinite(prevprevFp)) {
        return 2; // Dubious, but seems to qualify.
      }
      final UnifiedReal prevprev = UnifiedReal.valueOf(prevprevFp);
      checkComparable(prevprev, urVal);
      if (prevprev.compareTo(urVal) >= 0) {
        // urVal <= prevprev < prev < fpVal. fpVal is neither one of the
        // bracketing values, nor one next to it.
        return WRONG;
      } else {
        return 2;
      }
    } else {
      UnifiedReal prevDiff = urVal.subtract(prev);
      UnifiedReal fpValDiff = fpAsUr.subtract(urVal);
      checkComparable(fpValDiff, prevDiff);
      if (fpValDiff.compareTo(prevDiff) <= 0) {
        return 0;
      } else {
        return 1;
      }
    }
  }

  /**
   * Return the difference between fpVal and urVal in ulps. Behaves like ulpError(),
   * but accommodates situations in which urVal is not known comparable with rationals.
   * In that case the answer could conceivably be wrong, though we evaluate to sufficiently
   * high precision to make that unlikely.
   */
  private static int approxUlpError(double fpVal, UnifiedReal urVal) {
    assertTrue("Property wrong for " + urVal.toNiceString(),
        urVal.propertyCorrect(-1000));  // Check UnifiedReal for internal consistency.
    final UnifiedReal fpAsUr = UnifiedReal.valueOf(fpVal);
    final int errorSign = fpAsUr.compareTo(urVal, COMPARE_PREC);
    if (errorSign == 0) {
      return 0; // Exactly equal.
    }
    if (errorSign < 0) {
      return approxUlpError(-fpVal, urVal.negate());
    }
    // errorSign > 0
    final double prevFp = Math.nextAfter(fpVal, Double.NEGATIVE_INFINITY);
    if (Double.isInfinite(prevFp)) {
      // Most negative representable value was returned. True result is smaller.
      // That seems to qualify as "correctly rounded".
      return 0;
    }
    final UnifiedReal prev = UnifiedReal.valueOf(prevFp);
    if (prev.compareTo(urVal, COMPARE_PREC) >= 0) {
      // prev is a better approximation.
      final double prevprevFp = Math.nextAfter(prevFp, Double.NEGATIVE_INFINITY);
      if (Double.isInfinite(prevprevFp)) {
        return 2; // Dubious, but seems to qualify.
      }
      final UnifiedReal prevprev = UnifiedReal.valueOf(prevprevFp);
      if (prevprev.compareTo(urVal, COMPARE_PREC) >= 0) {
        // urVal <= prevprev < prev < fpVal. fpVal is neither one of the
        // bracketing values, nor one next to it.
        return WRONG;
      } else {
        return 2;
      }
    } else {
      UnifiedReal prevDiff = urVal.subtract(prev);
      UnifiedReal fpValDiff = fpAsUr.subtract(urVal);
      if (fpValDiff.compareTo(prevDiff, COMPARE_PREC) <= 0) {
        return 0;
      } else {
        return 1;
      }
    }
  }

  private static void assertTrue(String s, boolean b) {
    if (!b) {
      System.out.println(s);
      throw new AssertionError(s);
    }
  }

  private static void assertEquals(int x, int y) {
    if (x != y) {
      String s = "" + x + " != " + y;
      System.out.println(s);
      throw new AssertionError(s);
    }
  }

  private static UnifiedReal hypot(UnifiedReal x, UnifiedReal y) {
    return x.multiply(x).add(y.multiply(y)).sqrt();
  }

  /**
   * Generate a random double such that all bit patterns representing a finite value are equally
   * likely. Do not generate a NaN or Infinite result.
   */
  private static double getRandomDouble() {
    double result;
    do {
      result = Double.longBitsToDouble(rand.nextLong());
    } while (Double.isNaN(result) || Double.isInfinite(result));
    return result;
  }

  /**
   * Check that basic Math functions obey stated error bounds on argument x. x is assumed to be
   * finite. We assume that the UnifiedReal functions produce known rational results when the
   * results are rational.
   */
  private static void checkFunctionsAt(double x, double other) {
    if (Double.isNaN(x) || Double.isInfinite(x)) {
      return;
    }
    final UnifiedReal xAsUr = UnifiedReal.valueOf(x);
    final UnifiedReal otherAsUr = UnifiedReal.valueOf(other);
    if (x != 0.0) {
      assertTrue(
          "div 3: " + x, Double.isInfinite(3.0 / x) || ulpError(3.0 / x, THREE.divide(xAsUr)) == 0);
      assertTrue(
          "div -17: " + x,
          Double.isInfinite(-17.0 / x) || ulpError(-17.0 / x, MINUS17.divide(xAsUr)) == 0);
      assertTrue(
          "div " + other + ": " + x,
          !Double.isFinite(other / x) || ulpError(other / x, otherAsUr.divide(xAsUr)) == 0);
    }
    double result = Math.exp(x);
    if (result != 0 && !Double.isInfinite(result)) {
      assertTrue("exp: " + x, ulpError(result, xAsUr.exp()) <= 1);
    } // Otherwise the UnifiedReal computation may be intractible.
    if (x > 0) {
      assertTrue("ln: " + x, ulpError(Math.log(x), xAsUr.ln()) <= 1);
    }
    if (x > 0) {
      assertTrue("log10: " + x, ulpError(Math.log10(x), xAsUr.log()) <= 1);
    }
    if (x >= 0) {
      double rt = Math.sqrt(x);
      UnifiedReal urRt = xAsUr.sqrt();
      assertTrue("sqrt: " + x, ulpError(rt, urRt) == 0);
    }
    assertTrue("sin: " + x, ulpError(Math.sin(x), xAsUr.sin()) <= 1);
    assertTrue("cos: " + x, ulpError(Math.cos(x), xAsUr.cos()) <= 1);
    assertTrue("tan: " + x, ulpError(Math.tan(x), xAsUr.tan()) <= 1);
    assertTrue("atan: " + x, ulpError(Math.atan(x), xAsUr.atan()) <= 1);
    if (Math.abs(x) <= 1) {
      assertTrue("asin: " + x, ulpError(Math.asin(x), xAsUr.asin()) <= 1);
      assertTrue("acos: " + x, ulpError(Math.acos(x), xAsUr.acos()) <= 1);
    }
    if (Double.isNaN(other)) {
      return;
    }
    double h = Math.hypot(x, other);
    UnifiedReal hUr = hypot(xAsUr, otherAsUr);
    if (Double.isInfinite(h)) {
      double h2 = hUr.doubleValue();
      assertTrue(
          "inf hypot: " + x + ", " + other + ", hypot = " + h + ", hypot as UR = " + hUr
          + ", hypot from UR = " + h2,
          Double.isInfinite(h2)
              || Double.isInfinite(Math.nextAfter(h2, Double.POSITIVE_INFINITY)));
      // TODO: Since h2 is not yet correctly rounded, this could conceivably still fail
      // spuriously. But that's extremely unlikely.
    } else {
      final int error = ulpError(h, hUr);
      assertTrue("hypot: " + x + ", " + other, error <= 1);
    }
    if (x >= 0.0 || other == Math.rint(other)) {
      double p = Math.pow(x, other);
      if (!Double.isInfinite(p)) {
        UnifiedReal urP = xAsUr.pow(otherAsUr);
        if (urP.compareTo(UnifiedReal.valueOf(p), COMPARE_PREC) != 0) {
          assertTrue("pow: " + x + ", " + other, approxUlpError(p, urP) <= 1);
        }
      }
    }
  }

  private static void checkFunctionsAt(double x) {
    checkFunctionsAt(x, getRandomDouble());
  }

  /** Minimally test our testing infrastructure. */
  public static void checkUlpError() {
    for (double x = -1.0E-300 / 5.0; !Double.isInfinite(x); x *= -1.7) {
      // Sign changes every time, but absolute value increases; about 1000 iterations.
      double prev = Math.nextAfter(x, Double.NEGATIVE_INFINITY);
      double prevprev = Math.nextAfter(prev, Double.NEGATIVE_INFINITY);
      assertEquals(0, ulpError(x, UnifiedReal.valueOf(x)));
      assertEquals(2, ulpError(prev, UnifiedReal.valueOf(x)));
      assertEquals(2, ulpError(x, UnifiedReal.valueOf(prev)));
      assertEquals(WRONG, ulpError(prevprev, UnifiedReal.valueOf(x)));
      assertEquals(WRONG, ulpError(x, UnifiedReal.valueOf(prevprev)));
    }
  }

  public static void aFewSpecialDoubleChecks() {
    checkFunctionsAt(1.7976931348623157E308, -1.0128673137222576E307);
    // Fails for hypot() on OpenJDK:
    checkFunctionsAt(-2.6718173667255144E-307, -1.1432573432387167E-308);
    checkFunctionsAt(0.0);
    checkFunctionsAt(-0.0);
    checkFunctionsAt(1.0);
    checkFunctionsAt(-1.0);
    checkFunctionsAt(0.5);
    checkFunctionsAt(-0.5);
    checkFunctionsAt(Double.MIN_NORMAL);
    checkFunctionsAt(Double.MIN_VALUE);
    checkFunctionsAt(Double.MAX_VALUE);
  }

  public static void manyRandomDoubleChecks() {
    final boolean printTimes = false;
    final int nIters = 200;
    final long startTime = System.currentTimeMillis();
    for (int i = 0; i < nIters; i++) {
      double x = getRandomDouble();
      checkFunctionsAt(x);
    }
    final long finishTime = System.currentTimeMillis();
    if (printTimes) {
      System.err.println ("" + nIters + " iterations took " + (finishTime - startTime)
        + " msecs or " + (double)(finishTime - startTime) / (double)nIters + " msecs/iter");
    } else {
      System.out.println ("" + nIters + " iterations");
    }
  }

  public static void main(String[] args) {
    for (String s: args) {
      if (Character.isDigit(s.charAt(0))) {
        rand = new Random(Long.valueOf(s));
        break;
      }
    }
    if (rand == null) {
      rand = new Random();
    }
    checkUlpError();
    aFewSpecialDoubleChecks();
    manyRandomDoubleChecks();
  }
}
