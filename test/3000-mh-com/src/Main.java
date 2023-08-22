import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;

public class Main {

    public static void main(String[] args) throws Throwable {
      mh();
    }

    private static void mh() throws Throwable {
      METHOD.invokeExact(new A());
      // double res = (double)
      ANOTHER_METHOD.invokeExact(new A());
      // System.out.println(res);
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
    private static final MethodHandle METHOD;
    private static final MethodHandle ANOTHER_METHOD;

    static {
        try {
            A_FIELD = MethodHandles.lookup().findVarHandle(A.class, "field", int.class);
            METHOD = MethodHandles.lookup()
                      .findVirtual(
                          A.class,
                          "method",
                          MethodType.methodType(void.class));
            ANOTHER_METHOD = MethodHandles.lookup()
                      .findVirtual(A.class, "anotherMethod", MethodType.methodType(double.class));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    static class A {
        public int field;
        public void method() {
          System.out.println("method was called");
        }
        public double anotherMethod() {
          return 42.0d;
        }
    }
}
