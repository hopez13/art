## Comments for JSON configuration files, since the JSON format doesn't support it

### linker.config.json

Docs at
https://android.googlesource.com/platform/system/linkerconfig/+/master/README.md

* `permittedPaths`:
  * `/data` for JVMTI libraries used in ART testing; dalvikvm has to be able
    to dlopen them.

    TODO(b/129539670): Remove when there is proper support for native APEX tests.

  * `/system/framework` for framework odex files. dalvikvm has to be able to
    dlopen the files for CTS.

  * `/apex/com.android.art/javalib` for the primary boot image in the ART
    APEX, which is loaded through dlopen.
