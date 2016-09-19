#!/usr/bin/env python
#
# Copyright (C) 2016 The Android Open Source Project
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

import struct
import sys

if len(sys.argv) != 2:
    print "Usage: python rawdexdump.py <dexfile>"
    exit(0)

filename = sys.argv[1]
dex = open(filename, "rb")

def read_ubyte():
    return struct.unpack('B', dex.read(1))[0] 

def read_ushort():
    return struct.unpack('H', dex.read(2))[0] 

def read_uint():
    return struct.unpack('I', dex.read(4))[0] 

def read_uleb128():
    byte = read_ubyte()
    value = byte & 0x7F
    shift = 7
    while byte & 0x80 != 0:
        byte = read_ubyte()
        value = value | ((byte & 0x7f) << shift)
        shift += 7
    return value

def read_uleb128p1():
    return read_uleb128()-1

def read_sleb128():
    byte = read_ubyte()
    value = byte & 0x7F
    shift = 7
    while byte & 0x80 != 0:
        byte = read_ubyte()
        value = value | ((byte & 0x7f) << shift)
        shift += 7
    if byte & 0x40 != 0:
        value = value - (1 << shift)
    return value

def read_mutf8():
    str = ""
    b0 = read_ubyte()
    while b0 != 0:
        if b0 < 0x80:
            code = b0
        elif b0 & 0xe0 == 0xc0:
            b1 = read_ubyte()
            code = ((0x1f & b0) << 6) | (0x3f & b1)
        elif b0 & 0xf0 == 0xe0:
            b1 = read_ubyte()
            b2 = read_ubyte()
            code = ((0x0f & b0) << 12) | ((0x3f & b1) << 6) | (0x3f & b2)
        else:
            raise Exception("Unsupported leading MUTF-8 byte: %02x" % b0)
        str += unichr(code)
        b0 = read_ubyte()
    return str.encode("utf-8")

def peek_value(read_function):
    offset = dex.tell()
    value = read_function()
    dex.seek(offset)
    return value

# Parse a value from the current location in the dex file.
# Print the parsed value to stdout with its dex file offset and given label.
# Returns the parsed value.
def parse_value(label, read_function):
    offset = dex.tell()
    value = read_function()
    msg = "%08x " + label
    print msg % (offset, value)
    return value

def parse_uint(label):
    return parse_value(label, read_uint)

def parse_ushort(label):
    return parse_value(label, read_ushort)

def parse_ubyte(label):
    return parse_value(label, read_ubyte)

def parse_uleb128(label):
    return parse_value(label, read_uleb128)

def parse_uleb128p1(label):
    return parse_value(label, read_uleb128p1)

def parse_sleb128(label):
    return parse_value(label, read_sleb128)

def parse_mutf8(label):
    return parse_value(label, read_mutf8)

def parse_bytes(label, count):
    offset = dex.tell()
    value = dex.read(count)
    msg = "%08x " + label
    print msg % (offset, value)
    return value

def parse_none(label):
    print "         " + label

def align(x):
    while dex.tell() % x > 0:
        dex.read(1)

# Header
parse_bytes("magic: %s", 8)
parse_uint("checksum :%x")
parse_bytes("signature: %s", 20)
parse_uint("file_size: %d")
parse_uint("header_size: %d")
parse_uint("endian_tag: %x")
parse_uint("link_size: %d")
parse_uint("link_off: %08x")
map_off = parse_uint("map_off: %08x")
string_ids_size = parse_uint("string_ids_size: %d")
parse_uint("string_ids_off: %08x")
type_ids_size = parse_uint("type_ids_size: %d")
parse_uint("type_ids_off: %08x")
proto_ids_size = parse_uint("proto_ids_size: %d")
parse_uint("proto_ids_off: %08x")
field_ids_size = parse_uint("field_ids_size: %d")
parse_uint("field_ids_off: %08x")
method_ids_size = parse_uint("method_ids_size: %d")
parse_uint("method_ids_off: %08x")
class_defs_size = parse_uint("class_defs_size: %d")
parse_uint("class_defs_off: %08x")
parse_uint("data_size: %d")
parse_uint("data_off: %08x")

# string_ids
for x in range(0, string_ids_size):
    parse_uint("string_data_off %d: %%08x" % x)

# type_ids
for x in range(0, type_ids_size):
    parse_uint("descriptor_idx %d: %%d" % x)

# proto_ids
for x in range(0, proto_ids_size):
    parse_none("proto_id_item %d:" % x)
    parse_uint("  shorty_idx: %d")
    parse_uint("  return_type_idx: %d")
    parse_uint("  parameters_off: %08x")

# field_ids
for x in range(0, field_ids_size):
    parse_none("field_id_item %d:" % x)
    parse_ushort("  class_idx: %d")
    parse_ushort("  type_idx: %d")
    parse_uint("  name_idx: %d")

# method_ids
for x in range(0, method_ids_size):
    parse_none("method_id_item %d:" % x)
    parse_ushort("  class_idx: %d")
    parse_ushort("  proto_idx: %d")
    parse_uint("  name_idx: %d")

# class_defs
for x in range(0, class_defs_size):
    parse_none("class_def_item %d:" % x)
    parse_uint("  class_idx: %d")
    parse_uint("  access_flags: %08x")
    parse_uint("  superclass_idx: %d")
    parse_uint("  interfaces_off: %08x")
    parse_uint("  source_file_idx: %d")
    parse_uint("  annotations_off: %08x")
    parse_uint("  class_data_off: %08x")
    parse_uint("  static_values_off: %08x")

data_offset = dex.tell()

# read the map list so we know what's in the data section.
dex.seek(map_off)
map_size = read_uint()
map_contents = []
for x in range(0, map_size):
    type = read_ushort()
    unused = read_ushort()
    size = read_uint()
    offset = read_uint()
    if offset >= data_offset:
        map_contents.append((type, size, offset))

# data
TYPE_HEADER_ITEM = 0x0000
TYPE_STRING_ID_ITEM = 0x0001
TYPE_TYPE_ID_ITEM = 0x0002
TYPE_PROTO_ID_ITEM = 0x0003
TYPE_FIELD_ID_ITEM = 0x0004
TYPE_METHOD_ID_ITEM = 0x0005
TYPE_CLASS_DEF_ITEM = 0x0006
TYPE_MAP_LIST = 0x1000
TYPE_TYPE_LIST = 0x1001
TYPE_ANNOTATION_SET_REF_LIST = 0x1002
TYPE_ANNOTATION_SET_ITEM = 0x1003
TYPE_CLASS_DATA_ITEM = 0x2000
TYPE_CODE_ITEM = 0x2001
TYPE_STRING_DATA_ITEM = 0x2002
TYPE_DEBUG_INFO_ITEM = 0x2003
TYPE_ANNOTATION_ITEM = 0x2004
TYPE_ENCODED_ARRAY_ITEM = 0x2005
TYPE_ANNOTATIONS_DIRECTORY_ITEM = 0x2006

def describeType(x):
    if x == TYPE_HEADER_ITEM:
        return "TYPE_HEADER_ITEM"
    if x == TYPE_STRING_ID_ITEM:
        return "TYPE_STRING_ID_ITEM"
    if x == TYPE_TYPE_ID_ITEM:
        return "TYPE_TYPE_ID_ITEM"
    if x == TYPE_PROTO_ID_ITEM:
        return "TYPE_PROTO_ID_ITEM"
    if x == TYPE_FIELD_ID_ITEM:
        return "TYPE_FIELD_ID_ITEM"
    if x == TYPE_METHOD_ID_ITEM:
        return "TYPE_METHOD_ID_ITEM"
    if x == TYPE_CLASS_DEF_ITEM:
        return "TYPE_CLASS_DEF_ITEM"
    if x == TYPE_MAP_LIST:
        return "TYPE_MAP_LIST"
    if x == TYPE_TYPE_LIST:
        return "TYPE_TYPE_LIST"
    if x == TYPE_ANNOTATION_SET_REF_LIST:
        return "TYPE_ANNOTATION_SET_REF_LIST"
    if x == TYPE_ANNOTATION_SET_ITEM:
        return "TYPE_ANNOTATION_SET_ITEM"
    if x == TYPE_CLASS_DATA_ITEM:
        return "TYPE_CLASS_DATA_ITEM"
    if x == TYPE_CODE_ITEM:
        return "TYPE_CODE_ITEM"
    if x == TYPE_STRING_DATA_ITEM:
        return "TYPE_STRING_DATA_ITEM"
    if x == TYPE_DEBUG_INFO_ITEM:
        return "TYPE_DEBUG_INFO_ITEM"
    if x == TYPE_ANNOTATION_ITEM:
        return "TYPE_ANNOTATION_ITEM"
    if x == TYPE_ENCODED_ARRAY_ITEM:
        return "TYPE_ENCODED_ARRAY_ITEM"
    if x == TYPE_ANNOTATIONS_DIRECTORY_ITEM:
        return "TYPE_ANNOTATIONS_DIRECTORY_ITEM"
    return "???"

for (type, size, offset) in map_contents:
    dex.seek(offset)
    if type == TYPE_CODE_ITEM:
        for code_item in range(0, size):
            align(4)
            parse_none("code_item %d:" % code_item)
            parse_ushort("  registers_size: %d")
            parse_ushort("  ins_size: %d")
            parse_ushort("  outs_size: %d")
            tries_size = parse_ushort("  tries_size: %d")
            parse_uint("  debug_info_off: %08x")
            insns_size = parse_uint("  insns_size: %d")
            
            parse_none("  insns: ...") # TODO: Parse and display the instructions
            dex.read(insns_size * 2)
            if tries_size != 0 and insns_size % 2 == 1:
                parse_ushort("  padding: 0x%04x")
            if tries_size != 0:
                for try_item in range(0, tries_size):
                    parse_none("  try_item %d:" % try_item)
                    parse_uint("    start_addr: %d")
                    parse_ushort("    insn_count: %d")
                    parse_ushort("    handler_off: %d")
                parse_none("  encoded_catch_handler_list:")
                handlers_size = parse_uleb128("    size: %d")
                for encoded_catch_handler in range(0, handlers_size):
                    parse_none("    encoded_catch_handler %d:" % encoded_catch_handler)
                    size = parse_sleb128("      size: %d")
                    for encoded_type_addr_pair in range(0, abs(size)):
                        parse_none("      encoded_type_addr_pair: %d" % encoded_type_addr_pair)
                        parse_uleb128("        type_idx: %d")
                        parse_uleb128("        addr: %d")
                    if size <= 0:
                        parse_uleb128("      catch_all_addr: %d")

    elif type == TYPE_TYPE_LIST:
        for type_list in range(0, size):
            align(4)
            parse_none("type_list %d:" % type_list)
            size = parse_uint("  size: %d")
            for type_item in range(0, size):
                parse_ushort("  type_item %d: %%d" % type_item)

    elif type == TYPE_STRING_DATA_ITEM:
        for string_data_item in range(0, size):
            parse_none("string_data_item %d:" % string_data_item)
            parse_uleb128("  utf16_size: %d")
            parse_mutf8("  data: %s")

    elif type == TYPE_DEBUG_INFO_ITEM:
        for debug_info_item in range(0, size):
            parse_none("debug_info_item %d:" % debug_info_item)
            parse_uleb128("  line_start: %d")
            parameters_size = parse_uleb128("  parameters_size: %d")
            for parameter_name in range(0, parameters_size):
                parse_uleb128p1("  parameter_name %d: %%d" % parameter_name)
            bytecode = 1
            while bytecode != 0x00:
                bytecode = peek_value(read_ubyte)
                if bytecode == 0x00:
                    parse_ubyte("  0x%02x DBG_END_SEQUENCE")
                elif bytecode == 0x01:
                    parse_ubyte("  0x%02x DBG_ADVANCE_PC")
                    parse_uleb128("   addr_diff: %d")
                elif bytecode == 0x02:
                    parse_ubyte("  0x%02x DBG_ADVANCE_LINE")
                    parse_sleb128("    line_diff: %d")
                elif bytecode == 0x03:
                    parse_ubyte("  0x%02x DBG_START_LOCAL")
                    parse_uleb128("    register_num: %d")
                    parse_uleb128p1("    name_idx: %d")
                    parse_uleb128p1("    type_idx: %d")
                elif bytecode == 0x04:
                    parse_ubyte("  0x%02x DBG_START_LOCAL_EXTENDED")
                    parse_uleb128("    register_num: %d")
                    parse_uleb128p1("    name_idx: %d")
                    parse_uleb128p1("    type_idx: %d")
                    parse_uleb128p1("    sig_idx: %d")
                elif bytecode == 0x05:
                    parse_ubyte("  0x%02x DBG_END_LOCAL")
                    parse_uleb128("    register_num: %d")
                elif bytecode == 0x06:
                    parse_ubyte("  0x%02x DBG_RESTART_LOCAL")
                    parse_uleb128("    register_num: %d")
                elif bytecode == 0x07:
                    parse_ubyte("  0x%02x DBG_SET_PROLOGUE_END")
                elif bytecode == 0x08:
                    parse_ubyte("  0x%02x DBG_SET_EPILOGUE_BEGIN")
                elif bytecode == 0x09:
                    parse_ubyte("  0x%02x DBG_SET_FILE")
                    parse_uleb128p1("    name_idx: %d")
                else:
                    parse_ubyte("  0x%02x Special Opcode")

    elif type == TYPE_ANNOTATION_SET_REF_LIST:
        for annotation_set_ref_list in range(0, size):
            align(4)
            parse_none("annotation_set_ref_list %d:" % annotation_set_ref_list)
            size = parse_uint("  size: %d:")
            for annotation_set_ref_item in range(0, size):
                parse_none("  annotation_set_ref_item %d:" % annotation_set_ref_item)
                parse_uint("    annotations_off %d:")

    elif type == TYPE_ANNOTATION_SET_ITEM:
        for annotation_set_item in range(0, size):
            align(4)
            parse_none("annotation_set_item %d:" % annotation_set_item)
            size = parse_uint("  size: %d:")
            for annotation_off_item in range(0, size):
                parse_none("  annotation_off_item %d:" % annotation_off_item)
                parse_uint("    annotations_off %d:")

    elif type == TYPE_CLASS_DATA_ITEM:
        for class_data_item in range(0, size):
            parse_none("class_data_item %d:" % class_data_item)
            static_fields_size = parse_uleb128("  static_fields_size: %d")
            instance_fields_size = parse_uleb128("  instance_fields_size: %d")
            direct_methods_size = parse_uleb128("  direct_methods_size: %d")
            virtual_methods_size = parse_uleb128("  virtual_methods_size: %d")
            for static_field in range(0, static_fields_size):
                parse_none("  static_field %d:" % static_field)
                parse_uleb128("    field_idx_diff: %d")
                parse_uleb128("    access_flags: %d")
            for instance_field in range(0, instance_fields_size):
                parse_none("  instance_field %d:" % instance_field)
                parse_uleb128("    field_idx_diff: %d")
                parse_uleb128("    access_flags: %d")
            for direct_method in range(0, direct_methods_size):
                parse_none("  direct_method %d:" % direct_method)
                parse_uleb128("    method_idx_diff: %d")
                parse_uleb128("    access_flags: %d")
                parse_uleb128("    code_off: %08x")
            for virtual_method in range(0, virtual_methods_size):
                parse_none("  virtual_method %d:" % virtual_method)
                parse_uleb128("    method_idx_diff: %d")
                parse_uleb128("    access_flags: %d")
                parse_uleb128("    code_off: %08x")

    elif type == TYPE_MAP_LIST:
        for map_list in range(0, size):
            align(4)
            parse_none("map_list %d:" % map_list)
            size = parse_uint("size: %d ")
            for map_item in range(0, size):
                parse_none("map_item %d:" % map_item)
                type = peek_value(read_ushort)
                parse_ushort("  type: 0x%%04x %s" % describeType(type))
                parse_ushort("  unused: %04x")
                parse_uint("  size: %d")
                parse_uint("  offset: %08x")

    elif type == TYPE_ANNOTATIONS_DIRECTORY_ITEM:
        for annotations_directory_item in range(0, size):
            align(4)
            parse_none("annotations_directory_item %d:" % annotations_directory_item)
            parse_uint("  class_annotations_off: %08x")
            fields_size = parse_uint("  fields_size: %d")
            annotated_methods_size = parse_uint("  annotated_methods_size: %d")
            annotated_parameters_size = parse_uint(" annotated_parameters_size: %d")
            for field_annotation in range(0, fields_size):
                parse_none("  field_annotation %d:" % field_annotation)
                parse_uint("    field_idx: %d")
                parse_uint("    annotations_off: %08x")
            for method_annotation in range(0, annotated_methods_size):
                parse_none("  method_annotation %d:" % method_annotation)
                parse_uint("    method_idx: %d")
                parse_uint("    annotations_off: %08x")
            for parameter_annotation in range(0, annotated_parameters_size):
                parse_none("  parameter_annotation %d:" % parameter_annotation)
                parse_uint("    method_idx: %d")
                parse_uint("    annotations_off: %08x")

    elif type == TYPE_ANNOTATION_ITEM:
        # TODO: Parse these
        parse_none("annotation_items: ... (%d total)" % size)

    elif type == TYPE_ENCODED_ARRAY_ITEM:
        # TODO: Parse these
        parse_none("encoded_array_items: ... (%d total)" % size)

    else:
        raise Exception("TODO: type 0x%04x" % type)

