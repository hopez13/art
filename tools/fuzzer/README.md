# Building the ART Fuzzer

There are two ways to run one of the existing fuzzers: on host or on device.
Running for the device needs the full Android platform tree (aosp-main-with-phones).
Running for host can be done in the aosp-main-with-phones or the master-art
repository, the latter is smaller and faster to build, because it only has
the sources and dependencies required for the module.

In the following tutorial we will use an arm64 architecture and
the class verification fuzzer. 

We will set a shell variable with the fuzzer's name.
Their names can be found in the Android.bp file, under the cc_fuzz build rules.

```
FUZZER_NAME=libart_verify_classes_fuzzer
```

## Common steps for host and device:

1. Navigate to the root directory of the android repository.

2. From the console, set up the development environment.
```
source build/envsetup.sh
```


## Host

3. Build the fuzzer for x86
 ```
lunch armv8-trunk_staging-userdebug
```
```
SANITIZE_HOST=address make ${FUZZER_NAME}
```

In the first command, we use the trunk_staging release configuration. 
The command is composed of:
```
lunch <product>-trunk_staging-<variant>
```

4. Run the fuzzer
```
out/host/linux-x86/fuzz/x86_64/${FUZZER_NAME}/${FUZZER_NAME} \
out/host/linux-x86/fuzz/x86_64/${FUZZER_NAME}/corpus -print_pcs=1
```

The first part of the command is the fuzzer's path, followed by the
corpus and the argument `-print_pcs=1` which makes it more verbose.



## Device

3. Build the fuzzer for your device
```
lunch aosp_husky-trunk_staging-userdebug \
&&unset SOONG_ALLOW_MISSING_DEPENDENCIES \
&&unset TARGET_BUILD_UNBUNDLED \
&&unset BUILD_BROKEN_DISABLE_BAZEL
```

```
SANITIZE_HOST=address make ${FUZZER_NAME}
```

The first command is composed of:
```
lunch <product>-trunk_staging-<variant>
```
and 'husky' is the internal name for the device in this example.


4. Add the fuzzer's files on the device

```
adb root
```

```
adb sync data
```

5. Run the fuzzer

We are using an arm64 architecture, but it can be replaced with any other
supported architecture.


```
adb shell /data/fuzz/$(get_build_var TARGET_ARCH)/${FUZZER_NAME}/${FUZZER_NAME} \
/data/fuzz/$(get_build_var TARGET_ARCH)/${FUZZER_NAME}/corpus
```

The first part of the command is the fuzzer's path and the next one is the corpus.

## Corpus

The fuzzer uses a corpus as a starting point in order to generate new inputs
representing dex files.
Our current corpus contains an empty file, an empty main,
main that prints "Hello, world!", hand created dex file regression tests,
tests from our art test suite. Also, when the fuzzer generates a new
input, it proves that can cause a crash and offers more code coverage,
it is added to the existing corpus as a dex file.

The corpus used by the fuzzer can be cleaned with the command:

```
rm -rf out/host/linux-x86/fuzz/$(get_build_var TARGET_ARCH)/${FUZZER_NAME}/corpus
```

