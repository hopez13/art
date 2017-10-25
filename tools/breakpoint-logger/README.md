# breakpointlogger

breakpointlogger is a JVMTI agent that lets one set breakpoints that are logged
when they are hit.

# Usage
### Build
>    `make libbreakpointlogger`  # or 'make libbreakpointloggerd' with debugging checks enabled

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

### Command Line
#### ART
>    `art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so '-agentpath:libbreakpointlogger.so=Lclass/Name;->methodName()V@0' -cp tmp/java/helloworld.dex -Xint helloworld`

* `-Xplugin` and `-agentpath` need to be used, otherwise the agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.
* Multiple breakpoints can be included in the options, separated with ','s.

### Output
A normal run will look something like this:

    % ./test/run-test --host --dev --with-agent 'libbreakpointlogger.so=LMain;->main([Ljava/lang/String;)V@0' 001-HelloWorld
    <normal output removed>
    dalvikvm32 W 10-25 10:39:09 18063 18063 breakpointlogger.cc:277] Breakpoint at location: 0x00000000 in method LMain;->main([Ljava/lang/String;)V (source: Main.java:13) thread: main
    Hello, world!
