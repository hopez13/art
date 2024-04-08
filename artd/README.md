## artd

artd is a component of ART Service. It is a shim service to do tasks that
require elevated permissions that are not available to system_server, such as
manipulation of the file system and invoking dex2oat. It publishes a binder
interface that is internal to ART service's Java code. When it invokes other
binaries, it passes input and output files as FDs and drops capability before
exec.

### System properties

artd can be controlled by the system properties listed below. Note that the list
doesn't include options passed to dex2oat and other processes.

- `dalvik.vm.artd-verbose`: Log verbosity of the artd process. The syntax is the
  same as the runtime's `-verbose` flag.

### Pre-reboot mode

artd can be run in Pre-reboot mode through the `--pre-reboot` flag. The
Pre-reboot mode is for generating outputs for Pre-reboot Dexopt. The only
difference between the Pre-reboot mode and the normal mode is that, when
outputting files (dexopt artifacts and profiles), artd appends the ".staged"
suffix to the filename. Note that how artd addresses input files is solely
determined by AIDL arguments and is **not** affected by the Pre-reboot mode.
