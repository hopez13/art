# ART VIXL Simulator Integration

This file documents the use of the ART Simulator for running tests on ART. The
simulator enables us to run the ART run-tests without the need for a target
device. This helps to speed up the development/debug/test cycle. The full AOSP
source tree, as well as the partial master-art AOSP source tree, are supported.

## Quick User Guide
1. Set lunch target and setup environment:

    There are two environment variables that need to be set in order to build
    the simulator:
    - `ART_USE_RESTRICTED_MODE`: Setup ART in a restricted mode, disabling
                                 complex features, which allows the ART
                                 simulator to be gradually enabled.
    - `ART_USE_SIMULATOR`: Enables use of the simulator to run Arm64 tests on an
                           x86_64 host machine.

    Note that use of the ART simulator (via `ART_USE_SIMULATOR`) currently requires
    use of the restricted mode (via `ART_USE_RESTRICTED_MODE`).

    ```bash
    export ART_USE_RESTRICTED_MODE=true
    # ALLOW_NINJA_ENV needs to be set to allow ART_USE_SIMULATOR to be
    # available to all scripts invoked by soong.
    export ALLOW_NINJA_ENV=true
    export ART_USE_SIMULATOR=true
    source build/envsetup.sh; lunch armv8-eng
    ```

2. Build ART target and host:

    ```bash
    art/tools/buildbot-build.sh --target
    art/tools/buildbot-build.sh --host
    ```

3. Run Tests:

    To enable the simulator we use the `--simulator` flag. The simulator can
    be used directly with the dalvikvm or the ART test scripts.

    To run a single test on simulator, use the command:
    ```bash
    ./art/test.py --simulator --run-test --optimizing -t <TEST_NAME>
    ```

    To run all ART run-tests on simulator, use the `art/test.py` script with the
    following command:
    ```bash
    ./art/test.py --simulator --run-test --optimizing
    ```

4. Enable simulator tracing

    Simulator provides tracing feature which is useful in debugging. Setting
    runtime option `-verbose:simulator` will enable instruction trace and register
    updates.
    For example,
    ```bash
    ./art/test.py --simulator --optimizing --runtime-option=-verbose:simulator \
      -t 640-checker-simd
    ```

5. Debug

    Another useful usecase of the simulator is debugging using the `--gdb` flag.
    ```bash
    ./art/test.py --simulator --optimizing --gdb \
      -t 527-checker-array-access-split
    ```
    If developing a compiler optimization which affects the test case
    `527-checker-array-access-split`, you can use the simulator to run and
    generate the control flow graph with:
    ```bash
    ./art/test.py --simulator --optimizing --dump-cfg=oat.cfg --dex2oat-jobs 1 \
      -t 527-checker-array-access-split
    ```
