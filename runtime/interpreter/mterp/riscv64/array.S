%def op_aget(load="unimp", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET", wide="0", is_object="0"):
    unimp

%def op_aget_boolean():
%  op_aget(load="unimp", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aget_byte():
%  op_aget(load="unimp", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aget_char():
%  op_aget(load="unimp", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aget_object():
%  op_aget(load="unimp", shift="2", data_offset="MIRROR_OBJECT_ARRAY_DATA_OFFSET", wide="0", is_object="1")

%def op_aget_short():
%  op_aget(load="unimp", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aget_wide():
%  op_aget(load="unimp", shift="3", data_offset="MIRROR_WIDE_ARRAY_DATA_OFFSET", wide="1", is_object="0")

%def op_aput(store="unimp", shift="2", data_offset="MIRROR_INT_ARRAY_DATA_OFFSET", wide="0", is_object="0"):
    unimp

%def op_aput_boolean():
%  op_aput(store="unimp", shift="0", data_offset="MIRROR_BOOLEAN_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aput_byte():
%  op_aput(store="unimp", shift="0", data_offset="MIRROR_BYTE_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aput_char():
%  op_aput(store="unimp", shift="1", data_offset="MIRROR_CHAR_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aput_short():
%  op_aput(store="unimp", shift="1", data_offset="MIRROR_SHORT_ARRAY_DATA_OFFSET", wide="0", is_object="0")

%def op_aput_wide():
%  op_aput(store="unimp", shift="3", data_offset="MIRROR_WIDE_ARRAY_DATA_OFFSET", wide="1", is_object="0")

%def op_aput_object():
%  op_aput(store="unimp", shift="2", data_offset="MIRROR_OBJECT_ARRAY_DATA_OFFSET", wide="0", is_object="1")

%def op_array_length():
    unimp

// fill-array-data vAA, +BBBBBBBB
// Format 31t: AA|op bbbb(lo) BBBB(hi)
%def op_fill_array_data():
    EXPORT_PC
    FETCH a0, 1               // a0 := 0000.0000.0000.bbbb
    FETCH a1, 2, /*signed*/1  // a1 := ssss.ssss.ssss.BBBB
    slli  a1, a1, 16          // a1 := ssss.ssss.BBBB.0000
    or    a1, a0, a1          // a1 := ssss.ssss.BBBB.bbbb
    slli  a1, a1, 1           // a1 := +BBBBbbbb * 2
    add   a0, a1, xPC         // a0 := dex PC + (+BBBBbbbb) * 2
    srliw a1, xINST, 8        // a1 := AA
    GET_VREG a1, a1           // a1 := fp[AA]
    // artHandleFillArrayDataFromCode
    // a0: data array base address
    // a1: array reference
    // a2: xSELF (set up by trampoline)
    call art_quick_handle_fill_data

    // On return, continue to next opcode.
    FETCH_ADVANCE_INST 3
    GET_INST_OPCODE t0
    GOTO_OPCODE t0

%def op_filled_new_array(helper="nterp_filled_new_array"):
    unimp

%def op_filled_new_array_range():
%  op_filled_new_array(helper="nterp_filled_new_array_range")

// new-array vA, vB, class@CCCC
// Format 22c: B|A|op, CCCC
// Clobbers: t0, t1
%def op_new_array():
%   slow_get_class = add_slow_path(op_new_array_slow_get_class)
    EXPORT_PC
    FETCH_FROM_THREAD_CACHE a0, /*miss*/ .L${opcode}_trampoline_get_class
    TEST_IF_MARKING t0, .L${opcode}_slow_read_barrier

.L${opcode}_resume:
    srliw t0, xINST, 12  // B into LSB
    GET_VREG a1, t0  // a1 holds vB (array length) for alloc-array
    ld t0, THREAD_ALLOC_ARRAY_ENTRYPOINT_OFFSET(xSELF)  // args a0, a1
    jalr t0  // on return, a0 holds array object ref
    fence w, w  // publish array object
    slliw t0, xINST, 20  // A as MSB of word
    srliw t1, t0, 28  // lower A down into LSB, zext
    SET_VREG_OBJECT a0, t1
    FETCH_ADVANCE_INST 2  // advance xPC, load xINST
    GET_INST_OPCODE /*out*/t0
    GOTO_OPCODE /*in*/t0

.L${opcode}_trampoline_get_class:
    tail $slow_get_class

.L${opcode}_slow_read_barrier:
    call art_quick_read_barrier_mark_reg10
    tail .L${opcode}_resume

%def op_new_array_slow_get_class():
    mv a0, xSELF
    ld a1, 0(sp)
    mv a2, xPC
    call nterp_get_class
    tail .L${opcode}_resume

