readnum CALL GETRV HALT

:readnum
    0 SETRV
    :loop
        IN 48 SUB               ; ... ra -> ... ra ascii_digit -> ... ra ascii_digit 48 -> ... ra digit
        DUP end JLT             ; --> ... ra digit digit end -> ... ra digit 
        DUP 9 CMP end JGT       ; --> ra digit digit 9 --> ra digit cmp_res end -> ... ra digit  
        GETRV 10 MUL ADD SETRV  ; --> ... ra digit rv 10 -> ... ra digit (rv*10) -> ... ra (rv*10 + digit) -> ... ra
        loop JMP                ; -> ... ra loop -> ... ra
    :end
        DROP
        RET

:g_printnum_sign 1

;:printnum
;    SWAP
;    DUP not_zero JNE
;    48 OUT
;    RET
;    :not_zero
;    g_printnum_sign LOAD 1 g_printnum_sign SAVE 
;    DUP JGE pos
;    g_printnum_sign LOAD 1 NEG g_printnum_sign SAVE
;    :pos
;    0 SETRV
;    ; ... ra num
;    :printnum_loop
;        DUP 10 MOD 48 ADD
;        ; ... ra num ascii_digit
;        SWAP
;        ; ... ra ascii_digit num 
;        GETRV 1 ADD SETRV
;        ; ... ra ascii_digit num
;        10 DIV
;        ; ... ra ascii_digit (num / 10)
;        DUP print_cycle JGE
;        JMP printnum_loop
;    :print_cycle
;        ; while (RV > 0) { OUT }
