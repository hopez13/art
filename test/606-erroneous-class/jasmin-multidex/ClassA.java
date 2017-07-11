/*
 * class ErrClass {
  public void foo() {}
}

class ClassB {
  ErrClass g;
}
*/

public final class ClassA {
  public static void foo() {
    ClassB.g.foo();
  }
}
