VM test harness.

Use "art/test.py -r" to run all tests, or "art/test.py -r -t <number>"
to run a single test.  Run "art/test.py -h" to see command flags;
in particular, the tests can be run on the desktop, on a USB-attached
device, or using the desktop "reference implementation".

For most tests, the sources are in the "src" subdirectory.  Sources found
in the "src2" directory are compiled separately but to the same output
directory; this can be used to exercise "API mismatch" situations by
replacing class files created in the first pass.  The "src-ex" directory
is built separately, and is intended for exercising class loaders.
