import java.util.function.IntSupplier;

public class Main {

  public static void main(String... args) throws Throwable {
    Class returnIntClass = Class.forName("ReturnInt");
    Runnable returnInt = (Runnable) (returnIntClass.newInstance());

    returnInt.run();
  }
}