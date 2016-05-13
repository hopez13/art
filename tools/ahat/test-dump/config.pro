# The goal is to perform obfuscation and nothing else, to test that proguard
# deobfuscation is working properly.
-dontpreverify
-dontoptimize
-dontshrink

-keep public class Main {
  public static void main(java.lang.String[]);
}

-printmapping proguard.map
