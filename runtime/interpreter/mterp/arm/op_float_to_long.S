%include "arm/unopWider.S" {"instr":"bl      f2l_doconv"}

%break
/*
 * Convert the float in r0 to a long in r0/r1.
 *
 * We have to clip values to long min/max per the specification.  The
 * expected common case is a "reasonable" value that converts directly
 * to modest integer.  The EABI convert function isn't doing this for us.
 */
f2l_doconv:
    ubfx    r2, r0, #23, #8             @ grab the exponent
    cmp     r2, #0x4f                   @ MININT < x > MAXINT?
    blt     __aeabi_f2lz                @ tail call to convert float to long
    cmp     r2, #0xff                   @ NaN?
    beq     f2l_isNaN
    adds    r0, r0, r0                  @ sign bit to carry
    mov     r0, #0x7fffffff             @ assume maxint
    adc     r0, r0, r0                  @ convert to minint (0x80000000) if exp negative
    mov     pc, lr
f2l_isNaN:
    mov     r0, #0
    mov     pc, lr                      @ return 0 for NaN
