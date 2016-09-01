Bisection Bug Search
====================

Bisection Bug Search is a tool for finding compiler optimizations bugs. It
accepts a program which exposes a bug by producing incorrect output and expected
output for the program. It then attempts to narrow down the issue to a single
method and optimization pass under the assumption that interpreter is correct.

Given methods in order M0..Mn finds smallest i such that compiling Mi and
interpreting all other methods produces incorrect output. Then, given ordered
optimization passes P0..Pl, finds smallest j such that compiling Mi with passes
P0..Pj-1 produces expected output and compiling Mi with passes P0..Pj produces
incorrect output. Prints Mi and Pj.

How to run Bisection Bug Search
===============================

    bisection_search.py [-h] [-cp CLASSPATH] [--lib LIB] [--64]
                               [--dalvikvm-option [OPTION [OPTION ...]]]
                               [--arg [TEST_ARGS [TEST_ARGS ...]]] [--image IMAGE]
                               [--raw-cmd RAW_CMD] [--device]
                               [--expected-output EXPECTED_OUTPUT]
                               [--check-script CHECK_SCRIPT] [--verbose]
                               [classname]

    optional arguments:
      -h, --help            show this help message and exit

    dalvikvm command options:
      -cp CLASSPATH, --classpath CLASSPATH
                            classpath
      classname             name of class to run
      --lib LIB             lib to use, default: libart.so
      --64                  x64 mode
      --dalvikvm-option [OPTION [OPTION ...]]
                            additional dalvikvm option
      --arg [TEST_ARGS [TEST_ARGS ...]]
                            argument passed to test
      --image IMAGE         path to image
      --raw-cmd RAW_CMD     call dalvikvm with this command, ignore other command
                            options

    bisection options:
      --device              run on device
      --expected-output EXPECTED_OUTPUT
                            file containing expected output
      --check-script CHECK_SCRIPT
                            script comparing output and expected output
      --verbose             enable verbose output
