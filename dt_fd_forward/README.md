# dt_fds

dt_fds is a jdwpTransport library. This is made to be run with libadbjdwp.so 
art-plugin (adb-jdwp plugin). The adb-jdwp plugin will have control of a eventfd
file-descriptor that will be passed to this library through the address option.

When this transport tries to accept a connection it reads from the event-fd a
pair of file descriptors. The first of these is a socket that links to the
remote debugger over some sequence of forwards set-up by the adb-jdwp plugin.
The second is an eventfd(EFD_SEMAPHORE) that this transport and the adb-jdwp
plugin use to synchronize writes. This is needed because DDMS outgoing traffic
needs to be multiplexed onto the same file-descriptor and we don't want to
interleave packets.
