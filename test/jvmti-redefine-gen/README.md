# ART JVMTI redefinition Testing

This is support code for JVMTI redefinition testing. Since Redefinition testing requires that we
embedded a dex/class file into the test as the result of the redefinition these files assist with
automatically creating the redefinition.

## Basics

A redefinition test has a few parts.

```bash
% tree test/902-hello-transformation
test/902-hello-transformation
├── build
├── expected.txt
├── info.txt
├── redefine_targets
│   └── art
│       └── Transform902.java
├── run
└── src
    ├── art
    │   ├── Redefinition.java -> ../../../jvmti-common/Redefinition.java
    │   ├── Test902.java
    │   └── Transform902.java
    └── Main.java

4 directories, 9 files
```

The `expected.txt` and `info.txt` are the same as a normal `run-test`.

The `build` file should be a 1-liner that shells down to
`test/jvmti-redefine-gen/redefine_build.sh`. Normally one should be able to just copy the
existing one from a different test. The build file will use `redefine-gen.py` to generate a
`test_<test_number>_transforms.<base-filename>` class for every file under the `redefine_targets`
directory. Each of these classes will have a `public static final DEX_BYTES` and `public static
final CLASS_BYTES` fields containing, respectively, the `dex` and `class` representations of the
class in `redefine_targets`. One should use these to get the `dex` and `class` bytes in the main
test body.

Note the directory structure of `redefine_targets` must match `src`. 

Note the subpackages are not preserved in the `test_<test_number>_transforms` package. [^1]

[^1]: **TODO** We should probably make the generated `transforms` class reflect the full
filepath/package of the redefined class, e.g. it would be nice if the `DEX_BYTES` were in
`test_902_transform.art.Transform902`. This is needed before we can convert (ex) test 921.

### Example

For example in this case when compiled there will be a `test_902_transforms.Transform902` class
on the classpath with a `DEX_BYTES` and `CLASS_BYTES` static fields containing the dex and
class-files of `art.Transform902` as it appears in `redefine_targets`. The main test body in `src/art/Test902.java` uses these to redefine the class defined in `src/art/Test902.java`.

## Adding to CTS

To have this test run under CTS one must do the following.

1) Add any test files not being redefined to the `art_cts_test_library_common` filegroup in
`art/test/Android.bp`. One doesn't need to re-add any files from `art/test/jvmti-common`.

2) Add the initial versions of any classes being redefined to the
`art_cts_jvmti_redefine_initial` filegroup in `art/test/Android.bp`.

3) Add the redefined versions of any classes being redefined to the
`art_cts_jvmti_redefine_final` filegroup in `art/test/Android.bp`.

4) Add the name of all the generated `transforms` files to the list `art_cts_jvmti_redefine_data`
in `art/test/Android.bp`. This should generally be of the form
`test_<test_number>_transforms/<Transform_class_name>.java`.

5) Add the `expected.txt` file to the list in `expected_cts_outputs` in `art/test/Android.bp`.

6) Copy directory `cts/hostsidetests/jvmti/run-tests/test-902` to `cts/hostsidetests/jvmti/run-tests/test-<test_number>`.

7) Run the following 1-liner. `RUN_TEST_NUM=<test_number>; find $ANDROID_BUILD_TOP/cts/hostsidetests/jvmti/run-tests/test-$RUN_TEST_NUM -type f | while read f; do sed -i -E s:902:$RUN_TEST_NUM:g $f; done`