%def op_check_cast():
    unimp

%def op_check_cast_slow_path():
    unimp

%def op_instance_of():
    unimp

%def op_instance_of_slow_path():
    unimp

%def op_new_instance():
    unimp

// *** iget ***

%def op_iget(load="", wide="", is_object=""):
    unimp

%def op_iget_slow_path(load, wide, is_object):
    unimp

%def op_iget_boolean():
    unimp

%def op_iget_byte():
    unimp

%def op_iget_char():
    unimp

%def op_iget_short():
    unimp

%def op_iget_wide():
    unimp

%def op_iget_object():
    unimp

// *** iput ***

%def op_iput(wide="", is_object=""):
    unimp

%def op_iput_slow_path(wide, is_object):
    unimp

%def op_iput_boolean():
    unimp

%def op_iput_byte():
    unimp

%def op_iput_char():
    unimp

%def op_iput_short():
    unimp

%def op_iput_wide():
    unimp

%def op_iput_object():
    unimp

// *** sget ***

%def op_sget(load="", wide="", is_object=""):
    unimp

%def op_sget_slow_path(load, wide, is_object):
    unimp

%def op_sget_boolean():
    unimp

%def op_sget_byte():
    unimp

%def op_sget_char():
    unimp

%def op_sget_short():
    unimp

%def op_sget_wide():
    unimp

%def op_sget_object():
    unimp

// *** sput ***

// Clobbers: t0, t1
.macro WRITE_BARRIER_IF_OBJECT is_object, value, holder, nonce
    .if \is_object
    beqz \value, .Lskip_write_barrier_\nonce // No object, skip out.
    ld t0, THREAD_CARD_TABLE_OFFSET(xSELF)
    srli t1, \holder, CARD_TABLE_CARD_SHIFT
    add t1, t0, t1
    sb t0, (t1)
.Lskip_write_barrier_\nonce:
    .endif
.endm

.macro CLEAR_STATIC_VOLATILE_MARKER reg
    andi \reg, \reg, ~0x1
.endm

// Clobbers: t0, t1, t2, a0
%def op_sput(wide="0", is_object="0"):
%   slow_path = add_slow_path(op_sput_slow_path, wide, is_object, "s7")
    srliw t2, xINST, 8  // t2 := AA
    .if $wide
    unimp
    .else
    GET_VREG s7, t2  // s7 := v[AA]
    .endif
    // Fast path: NterpGetStaticField's resolved_field from thread-local cache.
    // Store cache value in a0 to match slow path's return from NterpGetStaticField.
    // Slow path: restores value in s7 for downstream.
    FETCH_FROM_THREAD_CACHE /*resolved_field*/a0, .L${opcode}_slow
.L${opcode}_resume:
    lw t0, ART_FIELD_OFFSET_OFFSET(a0)
    lw a0, ART_FIELD_DECLARING_CLASS_OFFSET(a0)  // a0 := holder
    add t0, t0, a0
    .if $wide
    unimp
    .else
    sw s7, (t0)
    WRITE_BARRIER_IF_OBJECT $is_object, /*value*/s7, /*holder*/a0, /*nonce*/${opcode}
    TEST_IF_MARKING t0, .L${opcode}_mark
.L${opcode}_mark_resume:
    .endif
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE t0
    GOTO_OPCODE t0
.L${opcode}_slow:
    call $slow_path  // slow path offset can exceed regular branch imm
    // returns to .L${opcode}_resume, or not at all
.L${opcode}_mark:
    call art_quick_read_barrier_mark_reg10  // a0
    j .L${opcode}_mark_resume

// Input
//   - wide: 0 or 1, 32 bit or 64 bit value
//   - is_object: 0 or 1, value or reference
//   - value_reg: holds value to store. Avoid clobber set.
// Output
//   - a0: holds resolved_field.
// Clobbers: t0, t1
%def op_sput_slow_path(wide, is_object, value_reg):
    // Args for nterp_get_static_field
    mv a0, xSELF
    ld a1, (sp)
    mv a2, xPC
    .if $is_object
    mv a3, $value_reg
    .else
    mv a3, zero
    .endif  // is_object
    EXPORT_PC
    call nterp_get_static_field  // returned a0 := resolved_field
    .if $is_object
    // Reload value, it may have moved.
    srliw $value_reg, xINST, 8  // $value_reg := AA
    GET_VREG $value_reg, $value_reg  // $value_reg := v[AA]
    .endif  // is_object

    // Test for volatile bit
    slli t0, a0, 63
    bltz t0, 1f
    call .L${opcode}_resume  // resume offset can exceed regular branch imm
1:
    // Volatile static store; free to use $value_reg and a0.
    CLEAR_STATIC_VOLATILE_MARKER a0
    lw t0, ART_FIELD_OFFSET_OFFSET(a0)
    lw a0, ART_FIELD_DECLARING_CLASS_OFFSET(a0)  // a0 := holder
    TEST_IF_MARKING t1, 3f
2:
    add t0, a0, t0
    // Ensure the volatile store is released. RISC-V does not have sw.rl yet.
    fence rw, w
    .if $wide
    unimp
    .else
    sw $value_reg, (t0)
    .endif  // wide
    WRITE_BARRIER_IF_OBJECT $is_object, $value_reg, /*holder*/a0, /*nonce*/slow_${opcode}

    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE t0
    GOTO_OPCODE t0
3:
    call art_quick_read_barrier_mark_reg10  // a0
    j 2b

%def op_sput_boolean():
    unimp

%def op_sput_byte():
    unimp

%def op_sput_char():
    unimp

%def op_sput_short():
    unimp

%def op_sput_wide():
    unimp

// sput-object vAA, field@BBBB
// Format 21c: AA|op BBBB
%def op_sput_object():
%   op_sput(wide="0", is_object="1")

