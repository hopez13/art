import java.io.PrintStream;
import java.lang.reflect.Array;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;
/* renamed from: FuzzerUtils  reason: default package */
/* loaded from: classes.dex */
public class FuzzerUtils {
    public static PrintStream out = System.out;
    public static Random random = new Random(1);
    public static long seed = 1;
    public static int UnknownZero = 0;
    public static AtomicLong runningThreads = new AtomicLong(0);

    public static void seed(long j) {
        random = new Random(j);
        seed = j;
    }

    public static int nextInt() {
        return random.nextInt();
    }

    public static long nextLong() {
        return random.nextLong();
    }

    public static float nextFloat() {
        return random.nextFloat();
    }

    public static double nextDouble() {
        return random.nextDouble();
    }

    public static boolean nextBoolean() {
        return random.nextBoolean();
    }

    public static byte nextByte() {
        return (byte) random.nextInt();
    }

    public static short nextShort() {
        return (short) random.nextInt();
    }

    public static char nextChar() {
        return (char) random.nextInt();
    }

    public static void init(boolean[] zArr, boolean z) {
        for (int i = 0; i < zArr.length; i++) {
            zArr[i] = i % 2 == 0 ? z : i % 3 == 0;
        }
    }

    public static void init(boolean[][] zArr, boolean z) {
        for (boolean[] zArr2 : zArr) {
            init(zArr2, z);
        }
    }

    public static void init(long[] jArr, long j) {
        for (int i = 0; i < jArr.length; i++) {
            jArr[i] = i % 2 == 0 ? i + j : j - i;
        }
    }

    public static void init(long[][] jArr, long j) {
        for (long[] jArr2 : jArr) {
            init(jArr2, j);
        }
    }

    public static void init(int[] iArr, int i) {
        for (int i2 = 0; i2 < iArr.length; i2++) {
            iArr[i2] = i2 % 2 == 0 ? i + i2 : i - i2;
        }
    }

    public static void init(int[][] iArr, int i) {
        for (int[] iArr2 : iArr) {
            init(iArr2, i);
        }
    }

    public static void init(short[] sArr, short s) {
        for (int i = 0; i < sArr.length; i++) {
            sArr[i] = (short) (i % 2 == 0 ? s + i : s - i);
        }
    }

    public static void init(short[][] sArr, short s) {
        for (short[] sArr2 : sArr) {
            init(sArr2, s);
        }
    }

    public static void init(char[] cArr, char c) {
        for (int i = 0; i < cArr.length; i++) {
            cArr[i] = (char) (i % 2 == 0 ? c + i : c - i);
        }
    }

    public static void init(char[][] cArr, char c) {
        for (char[] cArr2 : cArr) {
            init(cArr2, c);
        }
    }

    public static void init(byte[] bArr, byte b) {
        for (int i = 0; i < bArr.length; i++) {
            bArr[i] = (byte) (i % 2 == 0 ? b + i : b - i);
        }
    }

    public static void init(byte[][] bArr, byte b) {
        for (byte[] bArr2 : bArr) {
            init(bArr2, b);
        }
    }

    public static void init(double[] dArr, double d) {
        for (int i = 0; i < dArr.length; i++) {
            dArr[i] = i % 2 == 0 ? i + d : d - i;
        }
    }

    public static void init(double[][] dArr, double d) {
        for (double[] dArr2 : dArr) {
            init(dArr2, d);
        }
    }

    public static void init(float[] fArr, float f) {
        for (int i = 0; i < fArr.length; i++) {
            fArr[i] = i % 2 == 0 ? i + f : f - i;
        }
    }

    public static void init(float[][] fArr, float f) {
        for (float[] fArr2 : fArr) {
            init(fArr2, f);
        }
    }

    public static void init(Object[][] objArr, Object obj) {
        for (Object[] objArr2 : objArr) {
            init(objArr2, obj);
        }
    }

    public static void init(Object[] objArr, Object obj) {
        for (int i = 0; i < objArr.length; i++) {
            try {
                objArr[i] = obj.getClass().newInstance();
            } catch (Exception e) {
                objArr[i] = obj;
            }
        }
    }

    public static long checkSum(boolean[] zArr) {
        long j = 0;
        for (int i = 0; i < zArr.length; i++) {
            j += zArr[i] ? i + 1 : 0;
        }
        return j;
    }

    public static long checkSum(boolean[][] zArr) {
        long j = 0;
        for (boolean[] zArr2 : zArr) {
            j += checkSum(zArr2);
        }
        return j;
    }

    public static long checkSum(long[] jArr) {
        long j = 0;
        int i = 0;
        while (i < jArr.length) {
            int i2 = i + 1;
            long j2 = i2;
            j += (jArr[i] / j2) + (jArr[i] % j2);
            i = i2;
        }
        return j;
    }

    public static long checkSum(long[][] jArr) {
        long j = 0;
        for (long[] jArr2 : jArr) {
            j += checkSum(jArr2);
        }
        return j;
    }

    public static long checkSum(int[] iArr) {
        long j = 0;
        int i = 0;
        while (i < iArr.length) {
            int i2 = i + 1;
            j += (iArr[i] / i2) + (iArr[i] % i2);
            i = i2;
        }
        return j;
    }

    public static long checkSum(int[][] iArr) {
        long j = 0;
        for (int[] iArr2 : iArr) {
            j += checkSum(iArr2);
        }
        return j;
    }

    public static long checkSum(short[] sArr) {
        long j = 0;
        int i = 0;
        while (i < sArr.length) {
            int i2 = i + 1;
            j += (short) ((sArr[i] / i2) + (sArr[i] % i2));
            i = i2;
        }
        return j;
    }

    public static long checkSum(short[][] sArr) {
        long j = 0;
        for (short[] sArr2 : sArr) {
            j += checkSum(sArr2);
        }
        return j;
    }

    public static long checkSum(char[] cArr) {
        long j = 0;
        int i = 0;
        while (i < cArr.length) {
            int i2 = i + 1;
            j += (char) ((cArr[i] / i2) + (cArr[i] % i2));
            i = i2;
        }
        return j;
    }

    public static long checkSum(char[][] cArr) {
        long j = 0;
        for (char[] cArr2 : cArr) {
            j += checkSum(cArr2);
        }
        return j;
    }

    public static long checkSum(byte[] bArr) {
        long j = 0;
        int i = 0;
        while (i < bArr.length) {
            int i2 = i + 1;
            j += (byte) ((bArr[i] / i2) + (bArr[i] % i2));
            i = i2;
        }
        return j;
    }

    public static long checkSum(byte[][] bArr) {
        long j = 0;
        for (byte[] bArr2 : bArr) {
            j += checkSum(bArr2);
        }
        return j;
    }

    public static double checkSum(double[] dArr) {
        double d = 0.0d;
        int i = 0;
        while (i < dArr.length) {
            int i2 = i + 1;
            double d2 = i2;
            d += (dArr[i] / d2) + (dArr[i] % d2);
            i = i2;
        }
        return d;
    }

    public static double checkSum(double[][] dArr) {
        double d = 0.0d;
        for (double[] dArr2 : dArr) {
            d += checkSum(dArr2);
        }
        return d;
    }

    public static double checkSum(float[] fArr) {
        double d = 0.0d;
        int i = 0;
        while (i < fArr.length) {
            int i2 = i + 1;
            float f = i2;
            d += (fArr[i] / f) + (fArr[i] % f);
            i = i2;
        }
        return d;
    }

    public static double checkSum(float[][] fArr) {
        double d = 0.0d;
        for (float[] fArr2 : fArr) {
            d += checkSum(fArr2);
        }
        return d;
    }

    public static long checkSum(Object[][] objArr) {
        long j = 0;
        for (Object[] objArr2 : objArr) {
            j += checkSum(objArr2);
        }
        return j;
    }

    public static long checkSum(Object[] objArr) {
        long j = 0;
        for (int i = 0; i < objArr.length; i++) {
            j = (long) (j + (checkSum(objArr[i]) * Math.pow(2.0d, i)));
        }
        return j;
    }

    public static long checkSum(Object obj) {
        if (obj == null) {
            return 0L;
        }
        return obj.getClass().getCanonicalName().length();
    }

    public static byte[] byte1array(int i, byte b) {
        byte[] bArr = new byte[i];
        init(bArr, b);
        return bArr;
    }

    public static byte[][] byte2array(int i, byte b) {
        byte[][] bArr = (byte[][]) Array.newInstance(byte.class, i, i);
        init(bArr, b);
        return bArr;
    }

    public static short[] short1array(int i, short s) {
        short[] sArr = new short[i];
        init(sArr, s);
        return sArr;
    }

    public static short[][] short2array(int i, short s) {
        short[][] sArr = (short[][]) Array.newInstance(short.class, i, i);
        init(sArr, s);
        return sArr;
    }

    public static int[] int1array(int i, int i2) {
        int[] iArr = new int[i];
        init(iArr, i2);
        return iArr;
    }

    public static int[][] int2array(int i, int i2) {
        int[][] iArr = (int[][]) Array.newInstance(int.class, i, i);
        init(iArr, i2);
        return iArr;
    }

    public static long[] long1array(int i, long j) {
        long[] jArr = new long[i];
        init(jArr, j);
        return jArr;
    }

    public static long[][] long2array(int i, long j) {
        long[][] jArr = (long[][]) Array.newInstance(long.class, i, i);
        init(jArr, j);
        return jArr;
    }

    public static float[] float1array(int i, float f) {
        float[] fArr = new float[i];
        init(fArr, f);
        return fArr;
    }

    public static float[][] float2array(int i, float f) {
        float[][] fArr = (float[][]) Array.newInstance(float.class, i, i);
        init(fArr, f);
        return fArr;
    }

    public static double[] double1array(int i, double d) {
        double[] dArr = new double[i];
        init(dArr, d);
        return dArr;
    }

    public static double[][] double2array(int i, double d) {
        double[][] dArr = (double[][]) Array.newInstance(double.class, i, i);
        init(dArr, d);
        return dArr;
    }

    public static char[] char1array(int i, char c) {
        char[] cArr = new char[i];
        init(cArr, c);
        return cArr;
    }

    public static char[][] char2array(int i, char c) {
        char[][] cArr = (char[][]) Array.newInstance(char.class, i, i);
        init(cArr, c);
        return cArr;
    }

    public static Object[] Object1array(int i, Object obj) {
        Object[] objArr = new Object[i];
        init(objArr, obj);
        return objArr;
    }

    public static Object[][] Object2array(int i, Object obj) {
        Object[][] objArr = (Object[][]) Array.newInstance(Object.class, i, i);
        init(objArr, obj);
        return objArr;
    }

    public static boolean[] boolean1array(int i, boolean z) {
        boolean[] zArr = new boolean[i];
        init(zArr, z);
        return zArr;
    }

    public static boolean[][] boolean2array(int i, boolean z) {
        boolean[][] zArr = (boolean[][]) Array.newInstance(boolean.class, i, i);
        init(zArr, z);
        return zArr;
    }

    public static synchronized void runThread(Runnable runnable) {
        synchronized (FuzzerUtils.class) {
            final Thread thread = new Thread(runnable);
            thread.start();
            runningThreads.incrementAndGet();
            new Thread(new Runnable() { // from class: FuzzerUtils.1
                @Override // java.lang.Runnable
                public void run() {
                    try {
                        thread.join();
                        FuzzerUtils.runningThreads.decrementAndGet();
                    } catch (InterruptedException e) {
                    }
                }
            }).start();
        }
    }

    public static void joinThreads() {
        while (runningThreads.get() > 0) {
            try {
                Thread.sleep(1000L);
            } catch (InterruptedException e) {
            }
        }
    }
}
