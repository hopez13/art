import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.lang.invoke.WrongMethodTypeException;

public class Main {

    public static void main(String[] args) throws Throwable {
      mh();
    }

    private static void mh() throws Throwable {
      for (int i = 0; i < 50000; ++i) {
        VOID_METHOD.invokeExact(new A());
        int returnedInt = (int) RETURN_INT.invokeExact(new A());
        System.out.println("virtual IL method returned " + returnedInt);
        double returnedDouble = (double) RETURN_DOUBLE.invokeExact(new A());
        System.out.println("virtual DL method returned " + returnedDouble);

        try {
          RETURN_INT.invokeExact(new A());
          fail("Target method returns int, but it's ignored. WMTE should be thrown");
        } catch (WrongMethodTypeException ignored) {}

        int privateReturnedInt = (int) PRIVATE_RETURN_INT.invokeExact(new A());
        System.out.println("private IL method returned " + privateReturnedInt);
      }
    }

    private static void fail(String msg) {
      throw new AssertionError(msg);
    }

    private static final VarHandle A_FIELD;
    private static final MethodHandle VOID_METHOD;
    private static final MethodHandle RETURN_DOUBLE;
    private static final MethodHandle RETURN_INT;
    private static final MethodHandle PRIVATE_RETURN_INT;
    private static final MethodHandle STATIC_METHOD;
    private static final MethodHandle ANOTHER_STATIC_METHOD;

    static {
        try {
            A_FIELD = MethodHandles.lookup().findVarHandle(A.class, "field", int.class);
            VOID_METHOD = MethodHandles.lookup()
                      .findVirtual(
                          A.class,
                          "voidMethod",
                          MethodType.methodType(void.class));
            RETURN_DOUBLE = MethodHandles.lookup()
                      .findVirtual(A.class, "returnDouble", MethodType.methodType(double.class));
            RETURN_INT = MethodHandles.lookup()
                      .findVirtual(A.class, "returnInt", MethodType.methodType(int.class));
            PRIVATE_RETURN_INT = MethodHandles.privateLookupIn(A.class, MethodHandles.lookup())
                      .findVirtual(A.class, "privateReturnInt", MethodType.methodType(int.class));
            STATIC_METHOD = MethodHandles.lookup()
                      .findStatic(A.class, "staticMethod", MethodType.methodType(double.class, A.class));
            ANOTHER_STATIC_METHOD = MethodHandles.lookup()
                      .findStatic(A.class, "staticMethod", MethodType.methodType(double.class));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    static class A {
        public int field;
        public void voidMethod() {
          System.out.println("in voidMethod");
        }

        public double returnDouble() {
          System.out.println("in returnDouble");
          return 42.0d;
        }

        public int returnInt() {
          System.out.println("in returnInt");
          return 42;
        }

        private int privateReturnInt() {
          System.out.println("in privateReturnInt");
          return 1042;
        }

        public static double staticMethod(A a) {
          return 42.0d;
        }

        public static double staticMethod(double d) {
          return d;
        }

        public static double staticMethod() {
          return 41.0d;
        }
    }
}
