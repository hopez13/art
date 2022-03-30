public class Main {
    public static void main(String[] strArr) {
        for (int i = 0; i < 20000; i++) {
            try {
                byte[] bArr = new byte[16];
                int i12 = 30;
                int i13 = 2;
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
    }
}
