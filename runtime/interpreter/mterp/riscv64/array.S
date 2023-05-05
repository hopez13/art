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

%def op_fill_array_data():
    unimp

%def op_filled_new_array(helper="nterp_filled_new_array"):
    unimp

%def op_filled_new_array_range():
%  op_filled_new_array(helper="nterp_filled_new_array_range")

// new-array vA, vB, class@CCCC
// Format 22c: B|A|op, CCCC
// Clobbers: t0, t1
%def op_new_array():
    EXPORT_PC
    FETCH_FROM_THREAD_CACHE a0, /*miss*/ 2f
1:
    srliw t0, xINST, 12  // B into LSB
    andi t0, t0, 0xF  // Zero out higher bits
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
2:
    mv a0, xSELF
    ld a1, 0(sp)
    mv a2, xPC
    jal nterp_get_class
    j 1b

