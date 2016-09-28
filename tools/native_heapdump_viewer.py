#!/usr/bin/env python
#
# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Generates a human-interpretable view of a native heap dump from 'am dumpheap -n'."""

import os
import os.path
import re
import subprocess
import sys

usage = """
Usage:
1. Collect a native heap dump from the device. For example:
   $ adb shell stop
   $ adb shell setprop libc.debug.malloc.program app_process
   $ adb shell setprop libc.debug.malloc.options backtrace=64
   $ adb shell start
    (launch and use app)
   $ adb shell am dumpheap -n <pid> /data/local/tmp/native_heap.txt
   $ adb pull /data/local/tmp/native_heap.txt

2. Run the viewer:
   $ python native_heap_viewer.py [--symbols SYMBOL_DIR] native_heap.txt 
      SYMBOL_DIR is the directory containing the .so files with symbols.
                 Defaults to $ANDROID_PRODUCT_OUT/symbols
   This outputs a file with lines of the form:

      5831776  29.09% 100.00%    10532     71b07bc0b0 /system/lib64/libandroid_runtime.so Typeface_createFromArray frameworks/base/core/jni/android/graphics/Typeface.cpp:68

   5831776 is the total number of bytes allocated at this stack frame, which
   is 29.09% of the total number of bytes allocated and 100.00% of the parent
   frame's bytes allocated. 10532 is the total number of allocations at this
   stack frame. 71b07bc0b0 is the address of the stack frame.
"""

symboldir = os.getenv("ANDROID_PRODUCT_OUT") + "/symbols"

args = sys.argv[1:]
if len(args) > 1 and args[0] == "--symbols":
    symboldir = args[1]
    args = args[2:]

if len(args) != 1:
    print usage
    exit(0)

native_heap = args[0]

re_map = re.compile("(?P<start>[0-9a-f]+)-(?P<end>[0-9a-f]+) .... (?P<offset>[0-9a-f]+) [0-9a-f]+:[0-9a-f]+ [0-9]+ +(?P<name>.*)")

class Backtrace:
    def __init__(self, is_zygote, size, number, frames):
        self.is_zygote = is_zygote
        self.size = size
        self.number = number
        self.frames = frames

class Mapping:
    def __init__(self, start, end, offset, name):
        self.start = start
        self.end = end
        self.offset = offset
        self.name = name

class FrameDescription:
    def __init__(self, function, location, library):
        self.function = function
        self.location = location
        self.library = library


backtraces = []
mappings = []

for line in open(native_heap, "r"):
    splitted = line.split()
    if len(splitted) > 7 and splitted[0] == "z" and splitted [2] == "sz":
        is_zygote = splitted[1] != "1"
        size = int(splitted[3])
        num = int(splitted[5])
        frames = map(lambda x: int(x, 16), splitted[7:])
        backtraces.append(Backtrace(is_zygote, size, num, frames))
        continue

    m = re_map.match(line)
    if m:
        start = int(m.group('start'), 16)
        end = int(m.group('end'), 16)
        offset = int(m.group('offset'), 16)
        name = m.group('name')
        mappings.append(Mapping(start, end, offset, name))
        continue

# Return the mapping that contains the given address.
# Returns None if there is no such mapping.
def find_mapping(addr):
    min = 0
    max = len(mappings) - 1
    while True:
        if max < min:
            return None
        mid = (min + max) // 2
        if mappings[mid].end <= addr:
            min = mid + 1
        elif mappings[mid].start > addr:
            max = mid - 1
        else:
            return mappings[mid]

# Resolve address libraries and offsets.
# addr_offsets maps addr to .so file offset
# addrs_by_lib maps library to list of addrs from that library
# Resolved addrs maps addr to FrameDescription
addr_offsets = {}
addrs_by_lib = {}
resolved_addrs = {}
EMPTY_FRAME_DESCRIPTION = FrameDescription("???", "???", "???")
for backtrace in backtraces:
    for addr in backtrace.frames:
        if addr in addr_offsets:
            continue
        mapping = find_mapping(addr)
        if mapping:
            addr_offsets[addr] = addr - mapping.start + mapping.offset
            if not (mapping.name in addrs_by_lib):
                addrs_by_lib[mapping.name] = []
            addrs_by_lib[mapping.name].append(addr)
        else:
            resolved_addrs[addr] = EMPTY_FRAME_DESCRIPTION


# Resolve functions and line numbers
print "Resolving symbols using directory %s..." % symboldir
for lib in addrs_by_lib:
    sofile = symboldir + lib
    if os.path.isfile(sofile):
        lma = 0
        result = subprocess.check_output(["objdump", "-j", ".text", "-h", sofile])
        for line in result.split("\n"):
            splitted = line.split()
            if len(splitted) > 5 and splitted[1] == ".text":
                lma = int(splitted[5], 16)
                break

        input_addrs = ""
        for addr in addrs_by_lib[lib]:
            input_addrs += "%s\n" % hex(addr_offsets[addr] - lma)
        p = subprocess.Popen(["addr2line", "-C", "-j", ".text", "-e", sofile, "-f"], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
        result = p.communicate(input_addrs)[0]
        splitted = result.split("\n")
        for x in range(0, len(addrs_by_lib[lib])):
            function = splitted[2*x];
            location = splitted[2*x+1];
            resolved_addrs[addrs_by_lib[lib][x]] = FrameDescription(function, location, lib)

    else:
        print "%s not found for symbol resolution" % lib
        fd = FrameDescription("???", "???", lib)
        for addr in addrs_by_lib[lib]:
            resolved_addrs[addr] = fd

def addr2line(addr):
    if addr == "ZYGOTE" or addr == "APP":
        return FrameDescription("", "", "")

    return resolved_addrs[int(addr, 16)]

class AddrInfo:
    def __init__(self, addr):
        self.addr = addr
        self.size = 0
        self.number = 0
        self.children = {}

    def addStack(self, size, number, stack):
        self.size += size * number
        self.number += number
        if len(stack) > 0:
            child = stack[0]
            if not (child.addr in self.children):
                self.children[child.addr] = child
            self.children[child.addr].addStack(size, number, stack[1:])

zygote = AddrInfo("ZYGOTE")
app = AddrInfo("APP")

def display(indent, total, parent_total, node):
    fd = addr2line(node.addr)
    print "%9d %6.2f%% %6.2f%% %8d %s%s %s %s %s" % (node.size, 100*node.size/float(total), 100*node.size/float(parent_total), node.number, indent, node.addr, fd.library, fd.function, fd.location)
    children = sorted(node.children.values(), key=lambda x: x.size, reverse=True)
    for child in children:
        display(indent + "  ", total, node.size, child)

for backtrace in backtraces:
    stack = []
    for addr in backtrace.frames:
        stack.append(AddrInfo("%x" % addr))
    stack.reverse()
    if backtrace.is_zygote:
        zygote.addStack(backtrace.size, backtrace.number, stack)
    else:
        app.addStack(backtrace.size, backtrace.number, stack)

print ""
print "%9s %6s %6s %8s    %s %s %s %s" % ("BYTES", "%TOTAL", "%PARENT", "COUNT", "ADDR", "LIBRARY", "FUNCTION", "LOCATION")
display("", app.size, app.size + zygote.size, app)
print ""
display("", zygote.size, app.size + zygote.size, zygote)
print ""

