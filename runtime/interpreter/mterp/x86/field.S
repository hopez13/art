%default { }
    /*
     * General field read / write (iget-* iput-*).
     */
    REFRESH_INST ${opnum}                   # fix rINST to include opcode
    movl    rPC, OUT_ARG0(%esp)             # arg0: Instruction* inst
    movl    rINST, OUT_ARG1(%esp)           # arg1: uint16_t inst_data
    movl    rFP, OUT_ARG2(%esp)             # arg2: uint32_t* vregs
    movl    rSELF, %eax
    movl    %eax, OUT_ARG3(%esp)            # arg3: Thread* self
    call    SYMBOL($helper)
    testl   %eax, %eax
    jz      MterpPossibleException
    RESTORE_IBASE
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
