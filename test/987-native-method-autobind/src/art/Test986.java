/** TODO: http://go/java-style#javadoc */
package art;

class Test986 {

  static {
    // NB This is called before any setup is done so we don't need to worry about getting bind
    // events.
    Main.bindAgentJNIForClass(Test986.class);
  }
  Test986() {
  }
}
