public class Main {
  public static void main(String[] args) {
    for (int i = 0; i < 100000; i++) {
      try {
          byte[] arr = new byte[10];
          int x = 10;
          int y = 2;
          while (true) {
              if (x-- > 0) {
                  arr[y++] = 42;
              }
          }
      } catch (Throwable e) {}
    }
  }
}
