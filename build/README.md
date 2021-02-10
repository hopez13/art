# Building ART

The recommended way to build ART is to use the `master-art` manifest, which only
has the sources and dependencies required to build the ART module
`com.android.art.apex`. It can be installed on a device running S or later.


## Building as a module on `master-art`

1.  Check out the `master-art` tree:

    ```
    repo init -b master-art -u <repository url>
    ```

2.  Set up the development environment:

    ```
    lunch art_module_<arch>
    export TARGET_BUILD_APPS=com.android.art
    export SOONG_ALLOW_MISSING_DEPENDENCIES=true
    ```

    `<arch>` is the device architecture, one of `arm`, `arm64`, `x86`, or
    `x86_64`. Regardless of architecture, the build includes the usual host
    architectures, and 64/32-bit multilib for the 64-bit products.

    To build the debug variant of the module, specify `com.android.art.debug`
    for `TARGET_BUILD_APPS` instead. It is also possible to list both.

    TODO(b/179779520): Provide a better setup command.

3.  Build the module:

    ```
    m
    ```

4.  Install the module and reboot:

    ```
    adb install out/target/product/module_<arch>/system/apex/com.android.art.apex
    adb reboot
    ```


## Building as part of the base system image

NOTE: This method of building is slated to be obsoleted in favor of the
module build on `master-art` above (b/172480617).

1.  Check out a full Android platform tree and lunch the appropriate product the
    normal way.

2.  Ensure ART is built from source:

    ```
    export SOONG_CONFIG_art_module_source_build=true
    ```

    If this isn't set then the build may use prebuilts of ART and libcore that
    may be older than the sources.

3.  Build the system image:

    ```
    m droid
    ```

4.  Install and boot the system image the normal way.
