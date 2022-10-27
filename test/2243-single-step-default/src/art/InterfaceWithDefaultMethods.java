package art;

interface InterfaceWithDefaultMethods {
  default void doSomething() {
    String name = getClass().getName();
    System.out.println(name + "::doSomething");
  }
}
