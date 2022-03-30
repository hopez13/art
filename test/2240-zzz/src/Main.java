public class Main {
  public static void main(String[] args) {
    for (int i = 0; i < 100000; i++) {
      try {
          byte[] arr = new byte[3]; // increase => fauly ref is increased
          int x = 0;
          int y = 0;
          while (true) {
              //if (++x < 0) { // hangs
              //if (++x < 1) { // hangs
              //if (++x < 2) { // hangs
              //if (++x < 3) { // hangs
              //if (++x < 4) { // hangs
              //if (++x < 5) { // passes
              if (x++ < 4) { // crashes
                  arr[y] = 42;
                  y++;
              }
              //x++; // test passes
          }
      } catch (Throwable e) {}
    }
  }
}
