%def unused():
    ebreak

%def op_const():
    unimp

// const/16 vAA, #+BBBB
// Format 21s: AA|op BBBB
// Clobbers: t0, t1, t2, t3
%def op_const_16():
    FETCH t1, /*count*/1, /*signed*/1  // t1 := +BBBB
    srliw t2, xINST, 8   // t2 := AA
    FETCH_ADVANCE_INST 2 // advance xPC, load xINST
    GET_INST_OPCODE t3   // t3 := next opcode
    SET_VREG t2, t1      // fp[AA] := +BBBB
    GOTO_OPCODE t3       // continue to next

// const/4 vA, #+B
// Format 11n: B|A|op
// Clobbers: t0, t1, t2, t3
%def op_const_4():
    slliw t1, xINST, 16   // B as MSB of word
    sraiw t1, t1, 28      // lower down into LSB, apply sext
    slliw t2, xINST, 20   // A as MSB of word
    srliw t2, t2, 28      // lower down into LSB, apply zext
    FETCH_ADVANCE_INST 1  // advance xPC, load xINST
    GET_INST_OPCODE t3    // t3 holds next opcode
    SET_VREG t2, t1       // fp[A] := +B
    GOTO_OPCODE t3        // continue to next

%def op_const_high16():
    unimp

%def op_const_object(jumbo="0", helper="nterp_load_object"):
    FETCH_FROM_THREAD_CACHE /*object ref*/ a0, /*slow path*/2f
    TEST_IF_MARKING t0, 3f
1:
    srliw t1, xINST, 8  // t1 := AA
    SET_VREG_OBJECT a0, t1

    .if $jumbo
    unimp
    .else
    FETCH_ADVANCE_INST 2
    .endif
    GET_INST_OPCODE t0
    GOTO_OPCODE t0
2:
    EXPORT_PC
    mv a0, xSELF // a0 := Thread*
    ld a1, (sp)  // a1 := ArtMethod*
    mv a2, xPC   // a2 := ptr to dex pc
    call $helper
    j 1b
3:
    call art_quick_read_barrier_mark_reg10  // a0/x10
    j 1b

%def op_const_class():
    unimp
%def op_const_method_handle():
    unimp
%def op_const_method_type():
    unimp

// const-string vAA, string@BBBB
// Format 21c: AA|op BBBB
// Clobbers: t0, a0, a1, a2
%def op_const_string():
%   op_const_object()

%def op_const_string_jumbo():
    unimp
%def op_const_wide():
    unimp
%def op_const_wide_16():
    unimp
%def op_const_wide_32():
    unimp
%def op_const_wide_high16():
    unimp
%def op_monitor_enter():
    unimp
%def op_monitor_exit():
    unimp
%def op_move(is_object="0"):
    unimp
%def op_move_16(is_object="0"):
    unimp
%def op_move_exception():
    unimp
%def op_move_from16(is_object="0"):
    unimp
%def op_move_object():
    unimp
%def op_move_object_16():
    unimp
%def op_move_object_from16():
    unimp
%def op_move_result(is_object="0"):
    unimp
%def op_move_result_object():
    unimp
%def op_move_result_wide():
    unimp
%def op_move_wide():
    unimp
%def op_move_wide_16():
    unimp
%def op_move_wide_from16():
    unimp

%def op_nop():
    unimp

%def op_unused_3e():
%  unused()

%def op_unused_3f():
%  unused()

%def op_unused_40():
%  unused()

%def op_unused_41():
%  unused()

%def op_unused_42():
%  unused()

%def op_unused_43():
%  unused()

%def op_unused_73():
%  unused()

%def op_unused_79():
%  unused()

%def op_unused_7a():
%  unused()

%def op_unused_e3():
%  unused()

%def op_unused_e4():
%  unused()

%def op_unused_e5():
%  unused()

%def op_unused_e6():
%  unused()

%def op_unused_e7():
%  unused()

%def op_unused_e8():
%  unused()

%def op_unused_e9():
%  unused()

%def op_unused_ea():
%  unused()

%def op_unused_eb():
%  unused()

%def op_unused_ec():
%  unused()

%def op_unused_ed():
%  unused()

%def op_unused_ee():
%  unused()

%def op_unused_ef():
%  unused()

%def op_unused_f0():
%  unused()

%def op_unused_f1():
%  unused()

%def op_unused_f2():
%  unused()

%def op_unused_f3():
%  unused()

%def op_unused_f4():
%  unused()

%def op_unused_f5():
%  unused()

%def op_unused_f6():
%  unused()

%def op_unused_f7():
%  unused()

%def op_unused_f8():
%  unused()

%def op_unused_f9():
%  unused()

%def op_unused_fc():
%  unused()

%def op_unused_fd():
%  unused()

