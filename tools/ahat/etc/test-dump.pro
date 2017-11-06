
-keep public class Main {
  public static void main(java.lang.String[]);
}

# Produce useful obfuscated stack traces so we can test useful deobfuscation
# of stack traces.
-renamesourcefileattribute SourceFile
-keepattributes SourceFile,LineNumberTable

