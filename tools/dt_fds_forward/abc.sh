#!/bin/bash
art --gdbserver localhost:12345 --debug -Xplugin:libopenjdkjvmtid.so $@
