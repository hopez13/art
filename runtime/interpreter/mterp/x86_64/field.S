%default { }
    /*
     * General field read / write (iget-* iput-*).
     */
    REFRESH_INST ${opnum}                   # fix rINST to include opcode
    movq    rPC, OUT_ARG0                   # arg0: Instruction* inst
    movl    rINST, OUT_32_ARG1              # arg1: uint16_t inst_data
    movq    rFP, OUT_ARG2                   # arg2: uint32_t* vregs
    movq    rSELF, OUT_ARG3                 # arg3: Thread* self
    call    SYMBOL($helper)
    testq   %rax, %rax
    jz      MterpPossibleException
    ADVANCE_PC_FETCH_AND_GOTO_NEXT 2
