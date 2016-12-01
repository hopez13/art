%include "arm/unopWide.S" {"instr":"bl      d2l_doconv"}

%break
/*
 * Convert the double in r0/r1 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
d2l_doconv:
    ubfx    r2, r1, #20, #11            @ grab the exponent
    movw    r3, #0x43e
    cmp     r2, r3                      @ MINLONG < x > MAXLONG?
    blt     __aeabi_d2lz                @ tail call to convert double to long
    movw    r3, #0x7ff
    cmp     r2, r3
    beq     d2l_isNaN                   @ NaN?
    mov     r0, #0xffffffff             @ assume maxlong for lsw
    mov     r1, #0x7fffffff             @ assume maxlong for msw
    adds    r1, r1, r1                  @ sign bit to carry
    adc     r0, r0, r0
    adc     r1, r1, r1                  @ convert maxlong to minlong if exp negative
    mov     pc, lr                      @ return
d2l_isNaN:
    mov     r0, #0
    mov     r1, #0
    mov     pc, lr                      @ return 0 for NaN
