import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;

public class Main {

    public static void main(String[] args) throws Throwable {
      mh();
    }

    private static void mh() throws Throwable {
      VOID_METHOD.invokeExact(new A());
      int returnedInt = (int) RETURN_INT.invokeExact(new A());
      System.out.println("IL method returned " + returnedInt);
      double returnedDouble = (double) RETURN_DOUBLE.invokeExact(new A());
      System.out.println("DL method returned " + returnedDouble);
      // int i = (int) INTERFACE_METHOD.invokeExact(new CI());
      // System.out.println(i);
      double ignored = (double) STATIC_METHOD.invokeExact(new A());
      ignored = (double) ANOTHER_STATIC_METHOD.invokeExact();
    }

/*
    private static void vh() {
        VarHandle vh = A_FIELD;
        A a = new A();
        vh.set(a, 42);
        System.out.println(a.field);
    }
*/
    private static final VarHandle A_FIELD;
    private static final MethodHandle VOID_METHOD;
    private static final MethodHandle RETURN_DOUBLE;
    private static final MethodHandle RETURN_INT;
    private static final MethodHandle STATIC_METHOD;
    private static final MethodHandle ANOTHER_STATIC_METHOD;
    private static final MethodHandle INTERFACE_METHOD;

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
            STATIC_METHOD = MethodHandles.lookup()
                      .findStatic(A.class, "staticMethod", MethodType.methodType(double.class, A.class));
            ANOTHER_STATIC_METHOD = MethodHandles.lookup()
                      .findStatic(A.class, "staticMethod", MethodType.methodType(double.class));
            INTERFACE_METHOD = MethodHandles.lookup()
                      .findVirtual(I.class, "defMethod", MethodType.methodType(int.class));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    static class CI implements I {}

    interface I {
        default int defMethod() {
             return 42;
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

        public static double staticMethod(A a) {
          return 42.0d;
        }

        public static double staticMethod() {
          return 41.0d;
        }
    }
}
