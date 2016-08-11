Bisection Bug Search
====================

Bisection Bug Search is a tool for finding compiler optimizations bugs. It
accepts a program which exposes a bug by producing incorrect output and expected
correct output for the program. It then attempts to narrow down the issue to a
single method and optimization pass.

More specifically the tool tries to find such method M and pass P that when
compiling only method M with optimizations up to but _excluding_ pass P and
interpreting all other methods program produces correct output, but when
compiling only method M with optimizations up to and _including_ pass P and
interpreting all other methods program produces incorrect output.

How to run Bisection Bug Search
===============================

    bisection-search.py [-h] -cp CLASSPATH
                        [--correct-output CORRECT_OUTPUT] [--host] [--64]
                        [-d] [--dalvikvm-option [OPTION [OPTION ...]]]
                        [--verbose]
                        classname

    positional arguments:
      classname             name of class to run

    optional arguments:
      -h, --help            show this help message and exit
      -cp CLASSPATH, --classpath CLASSPATH
                            classpath
      --correct-output CORRECT_OUTPUT
                            file containing correct output
      --host                run on host
      --64                  x64 mode
      -d                    use libartd.so
      --dalvikvm-option [OPTION [OPTION ...]]
                            additional dalvikvm argument
      --verbose             enable verbose output
