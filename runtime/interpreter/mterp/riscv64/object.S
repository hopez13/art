%def op_check_cast():
    unimp

%def op_check_cast_slow_path():
    unimp

%def op_instance_of():
    unimp

%def op_instance_of_slow_path():
    unimp

// new-instance vAA, type@BBBB
// Format 21c: AA|op BBBB
%def op_new_instance():
    EXPORT_PC
    FETCH_FROM_THREAD_CACHE /*klass object*/a0, /*slow path*/3f
    TEST_IF_MARKING t0, 4f
1:
    ld t0, THREAD_ALLOC_OBJECT_ENTRYPOINT_OFFSET(xSELF)
    jalr t0
    fence w, w  // Publish klass object
2:
    srliw t1, xINST, 8  // t1 := AA
    SET_VREG_OBJECT a0, t1

    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE t0
    GOTO_OPCODE t0
3:
    // Slow path: create klass object with NterpAllocateObject
    mv a0, xSELF
    ld a1, (sp)
    mv a2, xPC
    call nterp_allocate_object
    j 2b  // slow path performs its own publication fence
4:
    // Mark path
    call art_quick_read_barrier_mark_reg10
    j 1b

// *** iget ***

%def op_iget(load="", wide="", is_object=""):
    unimp

%def op_iget_slow_path(load, wide, is_object):
    unimp

%def op_iget_wide():
    unimp

%def op_iget_object():
    unimp

%def op_iget_boolean():
    unimp

%def op_iget_byte():
    unimp

%def op_iget_char():
    unimp

%def op_iget_short():
    unimp

// *** iput ***

.macro CLEAR_INSTANCE_VOLATILE_MARKER reg
    andi \reg, \reg, ~0x8000  // reset bit #31
.endm

.macro VOLATILE_STORE_SEQCST width value addr
  .if \width == 8
    fence rw, w
    sb \value, (\addr)
    fence rw, rw
  .elseif \width == 16
    fence rw, w
    sw \value, (\addr)
    fence rw, rw
  .elseif \width == 32
    amoswap.w.rl \value, \value, (\addr)
  .else  // width == 64
    amoswap.d.rl \value, \value, (\addr)
  .endif  // width
.endm

// iput vA, vB, field@CCCC
// Format 22c: B|A|op CCCC
// Clobbers: t0
%def op_iput(width="32", is_object="0"):
    unimp

// Input
//   - width: bit width of value. One of 8, 16, 32, 64.
//   - is_object: 0 or 1, value or reference
//   - value_reg: holds value to store. Avoid clobber set.
// Output
//   - a0: holds resolved_field.
// Clobbers: t0
%def op_iput_slow_path(width, is_object, value_reg):
    // Args for nterp_get_instance_field_offset
    mv a0, xSELF
    ld a1, (sp)
    mv a2, xPC
    .if $is_object
    mv a3, $value_reg
    .else
    mv a3, zero
    .endif  // is_object
    EXPORT_PC
    call nterp_get_instance_field_offset  // returned a0 := resolved field offset

    .if $is_object
    // Reload object reference; it could have moved.
    srliw $value_reg, xINST, 8        // $value_reg := B|A
    andi $value_reg, $value_reg, 0xF  // $value_reg := A
    GET_VREG \value_reg, t0           // $value_reg := v[A]
    .endif  // is_object

    // Test for volatile bit
    slli t0, a0, 32
    bltz t0, 1f
    tail .L${opcode}_regular_resume  // resume offset can exceed regular branch imm
1:
    // Volatile instance store; free to use $value_reg and a0
    CLEAR_INSTANCE_VOLATILE_MARKER a0
    srliw t0, xINST, 12  // t0 := B
    GET_VREG t0, t0      // t0 := v[B]; object reference; destination
    bnez 2f
    tail common_errNullObject
2:
    add t0, a0, t0
    // Ensure the volatile store is released.
    VOLATILE_STORE_SEQCST $width, $value_reg, t0
    WRITE_BARRIER_IF_OBJECT $is_object, $value_reg, /*holder*/a0, /*nonce*/slow_${opcode}

    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE t0
    GOTO_OPCODE t0

%def op_iput_wide():
    unimp

// iput-object vA, vB, field@CCCC
// Format 22c: B|A|op CCCC
%def op_iput_object():
%  op_iput(is_object="1")

%def op_iput_boolean():
    unimp

%def op_iput_byte():
    unimp

%def op_iput_char():
    unimp

%def op_iput_short():
    unimp

// *** sget ***

%def op_sget(load="", wide="", is_object=""):
    unimp

%def op_sget_slow_path(load, wide, is_object):
    unimp

%def op_sget_wide():
    unimp

%def op_sget_object():
    unimp

%def op_sget_boolean():
    unimp

%def op_sget_byte():
    unimp

%def op_sget_char():
    unimp

%def op_sget_short():
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
%def op_sput(width="32", is_object="0"):
%   slow_path = add_slow_path(op_sput_slow_path, width, is_object, "s7")
    srliw t2, xINST, 8  // t2 := AA
    .if $width == 64
    unimp
    .else
    GET_VREG s7, t2  // s7 := v[AA]
    .endif
    // Fast path: NterpGetStaticField's resolved_field from thread-local cache.
    // Stores cache value in a0 to match slow path's return from NterpGetStaticField.
    // Slow path: updates s7 if is_object, for possible GC movement.
    FETCH_FROM_THREAD_CACHE /*resolved_field*/a0, .L${opcode}_slow

.L${opcode}_regular_resume:
    lw t0, ART_FIELD_OFFSET_OFFSET(a0)
    lw a0, ART_FIELD_DECLARING_CLASS_OFFSET(a0)  // a0 := holder
    TEST_IF_MARKING t1, .L${opcode}_mark

.L${opcode}_mark_resume:
    add t0, t0, a0
    .if $width == 8
      sb s7, (t0)
    .elseif $width == 16
      sh s7, (t0)
    .elseif $width == 32
      sw s7, (t0)
    .else  // width == 64
      sd s7, (t0)
    .endif  // width
    WRITE_BARRIER_IF_OBJECT $is_object, /*value*/s7, /*holder*/a0, /*nonce*/${opcode}
    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE t0
    GOTO_OPCODE t0

.L${opcode}_slow:
    tail $slow_path  // slow path offset can exceed regular branch imm

.L${opcode}_mark:
    call art_quick_read_barrier_mark_reg10  // a0
    j .L${opcode}_mark_resume

// Input
//   - width: bit width of value. One of 8, 16, 32, 64.
//   - is_object: 0 or 1, value or reference
//   - value_reg: holds value to store. Avoid clobber set.
// Output
//   - a0: holds resolved_field.
// Clobbers: t0, t1
%def op_sput_slow_path(width, is_object, value_reg):
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
    // Reload object reference; it could have moved.
    srliw $value_reg, xINST, 8       // $value_reg := AA
    GET_VREG $value_reg, $value_reg  // $value_reg := v[AA]
    .endif  // is_object

    // Test for volatile bit
    slli t0, a0, 63
    bltz t0, 1f
    tail .L${opcode}_regular_resume  // resume offset can exceed regular branch imm
1:
    // Volatile static store; free to use $value_reg and a0.
    CLEAR_STATIC_VOLATILE_MARKER a0
    lw t0, ART_FIELD_OFFSET_OFFSET(a0)
    lw a0, ART_FIELD_DECLARING_CLASS_OFFSET(a0)  // a0 := holder
    TEST_IF_MARKING t1, 3f
2:
    add t0, a0, t0
    // Ensure the volatile store is released.
    VOLATILE_STORE_SEQCST $width, $value_reg, t0
    WRITE_BARRIER_IF_OBJECT $is_object, $value_reg, /*holder*/a0, /*nonce*/slow_${opcode}

    FETCH_ADVANCE_INST 2
    GET_INST_OPCODE t0
    GOTO_OPCODE t0
3:
    call art_quick_read_barrier_mark_reg10  // a0
    j 2b

%def op_sput_wide():
    unimp

// sput-object vAA, field@BBBB
// Format 21c: AA|op BBBB
%def op_sput_object():
%   op_sput(width="32", is_object="1")

%def op_sput_boolean():
    unimp

%def op_sput_byte():
    unimp

%def op_sput_char():
    unimp

%def op_sput_short():
    unimp

