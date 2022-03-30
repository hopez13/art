import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.lang.reflect.Array;
/* renamed from: Test  reason: default package */
/* loaded from: classes.dex */
public class Main {
    public static final int N = 400;
    public static long instanceCount = 734783898;
    public static float fFld = 1.481f;
    public static volatile boolean bFld = true;
    public static long vMeth_check_sum = 0;
    public static long vMeth1_check_sum = 0;
    public static long iMeth_check_sum = 0;
    public static long vMeth2_check_sum = 0;

    public static void vMeth1(double d, long j, int i) {
        vMeth1_check_sum += Double.doubleToLongBits(d) + j + i + Float.floatToIntBits(1.0f);
    }

    public static void vMeth2(int i) {
        int i2 = 293;
        while (9 < i2) {
            i = (int) (i + (i2 - 0.839f));
            i2--;
        }
        float f = 0.839f + i;
        int i3 = 12;
        int i4 = 78;
        int i5 = -73;
        int i6 = 17445;
        double d = 95.7336d;
        while (i3 < 201) {
            instanceCount += f;
            i *= i;
            i5 = i3;
            while (2 > i5) {
                i6 &= i3;
                i += (int) f;
                d = 1.0d;
                i5 += 3;
                i4 = 0;
            }
            i3++;
        }
        vMeth2_check_sum += i + i2 + 9 + Float.floatToIntBits(f) + i3 + i4 + i5 + i6 + Double.doubleToLongBits(d) + 44670 + 0;
    }

    public static int iMeth(int i, double d) {
        long[] jArr = new long[N];
        FuzzerUtils.init(jArr, 19666L);
        jArr[41] = jArr[41] * i;
        vMeth2(3);
        long doubleToLongBits = ((((i + 1) + 60) + Double.doubleToLongBits(d)) - 681213165) + 110 + FuzzerUtils.checkSum(jArr);
        iMeth_check_sum += doubleToLongBits;
        return (int) doubleToLongBits;
    }

    public static void vMeth(byte b, int i, double d) {
        int[][] iArr = (int[][]) Array.newInstance(int.class, N, N);
        double[] dArr = new double[N];
        float[] fArr = new float[N];
        FuzzerUtils.init(iArr, -65045);
        FuzzerUtils.init(dArr, 70.122426d);
        FuzzerUtils.init(fArr, 2.463f);
        int i2 = iArr[(i >>> 1) % N][84];
        long j = instanceCount;
        int i3 = (int) j;
        int i4 = (i3 >>> 1) % N;
        double d2 = dArr[i4] - 1.0d;
        dArr[i4] = d2;
        vMeth1(d2, Long.reverseBytes(j), i3);
        vMeth_check_sum += (((((b + (i3 - 1)) + Double.doubleToLongBits(d)) - 35758) + ((int) instanceCount)) - 6) + 0 + 1 + FuzzerUtils.checkSum(iArr) + Double.doubleToLongBits(FuzzerUtils.checkSum(dArr)) + Double.doubleToLongBits(FuzzerUtils.checkSum(fArr));
    }

    /* JADX WARN: Type inference failed for: r0v20, types: [java.lang.Throwable] */
    public void mainTest(String[] strArr) {
//        long[][] jArr;
        double d;
        int i;
        char c;
        int[] iArr = new int[N];
        double[] dArr = new double[N];
        FuzzerUtils.init((long[][]) Array.newInstance(long.class, N, N), -3481106114L);
        FuzzerUtils.init(iArr, 46199);
        FuzzerUtils.init(dArr, 2.106973d);
        int i2 = -74;
        int i3 = 30076;
        int i4 = -12;
        int i5 = 2;
        int i6 = 3;
        while (true) {
            d = 30.123993d;
            if (i6 >= 237) {
                break;
            }
            i2 -= i2;
            instanceCount = (((10 - i2) * 7) * i6) ^ instanceCount;
            if (bFld) {
                vMeth((byte) 50, i6, 30.123993d);
                c = 2;
            } else {
                int i7 = 2;
                while (i7 < 43) {
                    i5 = 1;
                    while (i5 < 2) {
                        i4 += i5 * i5;
                        i5++;
                    }
                    i4 *= (int) instanceCount;
                    i2 += (i7 * i7) + 152;
                    i7++;
                }
                c = 2;
                i3 = i7;
            }
            i6++;
            iArr = iArr;
        }
        long j = instanceCount;
        int i8 = (int) j;
        instanceCount = j;
        int i9 = 3;
        while (i9 < 262) {
            i4 = (int) d;
            instanceCount += (i9 * i9) + 49;
            iArr[i9 - 1] = -90;
            i9++;
            d = 30.123993d;
        }
        PrintStream printStream = System.out;
        PrintStream printStream2 = System.err;
        PrintStream printStream3 = new PrintStream(new OutputStream() { // from class: Test.1
            @Override // java.io.OutputStream
            public void write(int i10) throws IOException {
            }
        });
        System.setOut(printStream3);
        System.setErr(printStream3);
        boolean z = false;
        int i10 = 12;
        for (int i11 = -49654; i11 < 611086; i11 += 18) {
            if (!z) {
                System.setOut(printStream);
                System.setErr(printStream2);
                System.setOut(printStream3);
                System.setErr(printStream3);
                z = true;
                i10 = 1;
            } else {
                z = z;
            }
            try {
                byte[] bArr = new byte[16];
                int i12 = i4;
                int i13 = i5;
                while (true) {
                    int i14 = i12 - 1;
                    if (i12 > 0) {
                        int i15 = i13 + 1;
                        bArr[i13] = -1;
                        i13 = i15;
                        i12 = i14;
                    }
                }
            } catch (Throwable th) {
            }
        }
        System.setOut(printStream);
        System.setErr(printStream2);
        double d2 = 30.123993d;
        while (true) {
            i = i10 + 1;
            if (i >= 232) {
                break;
            }
            d2 = -90;
            dArr[i + 1] = instanceCount;
            i10 = i;
            i8 = i4;
        }
        double d3 = 1.0d;
        int i16 = 31190;
        double d4 = 1.0d;
        while (true) {
            d4 += d3;
            if (d4 < 230.0d) {
                instanceCount %= -89;
                i4 >>= -253;
                fFld -= i3;
                i16 += i6;
                d3 = 1.0d;
            } else {
                FuzzerUtils.out.println("i i1 d4 = " + i6 + "," + i8 + "," + Double.doubleToLongBits(d2));
                FuzzerUtils.out.println("i17 i18 i19 = " + i3 + "," + i4 + "," + i5);
                FuzzerUtils.out.println("i20 i21 i22 = " + i16 + "," + i9 + ",45172");
                FuzzerUtils.out.println("by3 i23 d5 = -90," + i + "," + Double.doubleToLongBits(d4));
//                FuzzerUtils.out.println("lArr1 iArr1 dArr1 = " + FuzzerUtils.checkSum(jArr) + "," + FuzzerUtils.checkSum(iArr) + "," + Double.doubleToLongBits(FuzzerUtils.checkSum(dArr)));
                FuzzerUtils.out.println("Test.instanceCount Test.fFld Test.bFld = " + instanceCount + "," + Float.floatToIntBits(fFld) + "," + (bFld ? 1 : 0));
                FuzzerUtils.out.println("vMeth1_check_sum: " + vMeth1_check_sum);
                FuzzerUtils.out.println("vMeth2_check_sum: " + vMeth2_check_sum);
                FuzzerUtils.out.println("iMeth_check_sum: " + iMeth_check_sum);
                FuzzerUtils.out.println("vMeth_check_sum: " + vMeth_check_sum);
                return;
            }
        }
    }

    public static void main(String[] strArr) {
        try {
            Main test = new Main();
            for (int i = 0; i < 10; i++) {
                test.mainTest(strArr);
            }
        } catch (Exception e) {
            FuzzerUtils.out.println(e.getClass().getCanonicalName());
        }
    }
}
