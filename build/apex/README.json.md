## Comments for JSON configuration files, since the JSON format doesn't support it

### linker.config.json

Docs at
https://android.googlesource.com/platform/system/linkerconfig/+/master/README.md#apex-linker-configuration

* `permittedPaths`:
  * `/data` for JVMTI libraries used in ART testing; dalvikvm has to be able
    to dlopen them.

    TODO(b/129534335): Move this to the linker configuration of the Test ART
    APEX when it is available.

  * `/system/framework` for framework odex files. dalvikvm has to be able to
    dlopen the files for CTS.

  * `/apex/com.android.art/javalib` for the primary boot image in the ART
    APEX, which is loaded through dlopen.
