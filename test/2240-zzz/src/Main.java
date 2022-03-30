public class Main {
    public static void main(String[] strArr) {
        for (int i = 0; i < 10000; i++) {
            try {
                byte[] bArr = new byte[10];
                int x = 30;
                int y = 2;
                while (true) {
                    int xx = x - 1;
                    if (x > 0) {
                        int yy = y + 1;
                        bArr[y] = -1;
                        y = yy;
                        x = xx;
                    }
                }
            } catch (Throwable th) {
            }
        }
    }
}
