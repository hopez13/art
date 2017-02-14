package java.lang;

import java.math.*;

public final class Long extends Number implements Comparable<Long> {
    public static final long MIN_VALUE = 0;
    public static final long MAX_VALUE = 0;
    public static final Class<Long> TYPE = null;

    static { }

    // Intrinsic! Do something cool. Return i + 1
    public static long highestOneBit(long i) {
      return i + 1;
    }

    // Intrinsic! Do something cool. Return i - 1
    public static long lowestOneBit(long i) {
      return i - 1;
    }

    // Intrinsic! Do something cool. Return i + i
    public static int numberOfLeadingZeros(long i) {
      return (int)(i + i);
    }

    // Intrinsic! Do something cool. Return i & (i >>> 1);
    public static int numberOfTrailingZeros(long i) {
      return (int)(i & (i >>> 1));
    }

    // Intrinsic! Do something cool. Return 5
     public static int bitCount(long i) {
       return 5;
     }

    // Intrinsic! Do something cool. Return i
    public static long rotateLeft(long i, int distance) {
      return i;
    }

    // Intrinsic! Do something cool. Return 10 * i
    public static long rotateRight(long i, int distance) {
      return 10 * i;
    }

    // Intrinsic! Do something cool. Return -i
    public static long reverse(long i) {
      return -i;
    }

    // Intrinsic! Do something cool. Return 0
    public static int signum(long i) {
      return 0;
    }

    // Intrinsic! Do something cool. Return 0
    public static long reverseBytes(long i) {
      return 0;
    }

    public String toString() {
      return "Redefined Long! value (as double)=" + ((double)value);
    }

    public static String toString(long i, int radix) {
      throw new Error("Method redefined away!");
    }
    public static String toUnsignedString(long i, int radix) {
      throw new Error("Method redefined away!");
    }

    private static BigInteger toUnsignedBigInteger(long i) {
      throw new Error("Method redefined away!");
    }
    public static String toHexString(long i) {
      throw new Error("Method redefined away!");
    }

    public static String toOctalString(long i) {
      throw new Error("Method redefined away!");
    }

    public static String toBinaryString(long i) {
      throw new Error("Method redefined away!");
    }

    static String toUnsignedString0(long val, int shift) {
      throw new Error("Method redefined away!");
    }

    static int formatUnsignedLong(long val, int shift, char[] buf, int offset, int len) {
      throw new Error("Method redefined away!");
    }

    public static String toString(long i) {
      throw new Error("Method redefined away!");
    }

    public static String toUnsignedString(long i) {
      throw new Error("Method redefined away!");
    }

    static void getChars(long i, int index, char[] buf) {
      throw new Error("Method redefined away!");
    }

    static int stringSize(long x) {
      throw new Error("Method redefined away!");
    }

    public static long parseLong(String s, int radix) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }

    public static long parseLong(String s) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }

    public static long parseUnsignedLong(String s, int radix) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }

    public static long parseUnsignedLong(String s) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }

    public static Long valueOf(String s, int radix) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }
    public static Long valueOf(String s) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }
    public static Long valueOf(long l) {
      throw new Error("Method redefined away!");
    }

    public static Long decode(String nm) throws NumberFormatException {
      throw new Error("Method redefined away!");
    }

    private final long value;

    public Long(long value) {
      this.value = value;
      throw new Error("Method redefined away!");
    }

    public Long(String s) throws NumberFormatException {
      this(0);
      throw new Error("Method redefined away!");
    }

    public byte byteValue() {
      throw new Error("Method redefined away!");
    }

    public short shortValue() {
      throw new Error("Method redefined away!");
    }

    public int intValue() {
      throw new Error("Method redefined away!");
    }

    public long longValue() {
      throw new Error("Method redefined away!");
    }

    public float floatValue() {
      throw new Error("Method redefined away!");
    }

    public double doubleValue() {
      throw new Error("Method redefined away!");
    }

    public int hashCode() {
      throw new Error("Method redefined away!");
    }

    public static int hashCode(long value) {
      throw new Error("Method redefined away!");
    }

    public boolean equals(Object obj) {
      throw new Error("Method redefined away!");
    }

    public static Long getLong(String nm) {
      throw new Error("Method redefined away!");
    }

    public static Long getLong(String nm, long val) {
      throw new Error("Method redefined away!");
    }

    public static Long getLong(String nm, Long val) {
      throw new Error("Method redefined away!");
    }
    public int compareTo(Long anotherLong) {
      throw new Error("Method redefined away!");
    }

    public static int compare(long x, long y) {
      throw new Error("Method redefined away!");
    }

    public static int compareUnsigned(long x, long y) {
      throw new Error("Method redefined away!");
    }

    public static long divideUnsigned(long dividend, long divisor) {
      throw new Error("Method redefined away!");
    }

    public static long remainderUnsigned(long dividend, long divisor) {
      throw new Error("Method redefined away!");
    }

    public static final int SIZE = 64;
    public static final int BYTES = SIZE / Byte.SIZE;

    public static long sum(long a, long b) {
      throw new Error("Method redefined away!");
    }

    public static long max(long a, long b) {
      throw new Error("Method redefined away!");
    }
    public static long min(long a, long b) {
      throw new Error("Method redefined away!");
    }

    private static final long serialVersionUID = 0;
}
