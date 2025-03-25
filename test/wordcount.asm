; sep = {',', '.', ';', '?', '!', '\t', ' '} = {44, 46, 59, 63, 33, 9, 32}

main JMP

:g_is_inside_word 0

:wc
    0 SETRV
    wc_loop JMP
    :wc_reset_g_inside_word
        0 g_is_inside_word SAVE
    :wc_loop
        IN
        DUP 44 CMP wc_reset_g_inside_word JEQ
        DUP 46 CMP wc_reset_g_inside_word JEQ
        DUP 59 CMP wc_reset_g_inside_word JEQ
        DUP 63 CMP wc_reset_g_inside_word JEQ
        DUP 33 CMP wc_reset_g_inside_word JEQ
        DUP 32 CMP wc_reset_g_inside_word JEQ
        DUP 9 CMP wc_reset_g_inside_word JEQ

        DUP 10 CMP wc_end JEQ
        DUP 13 CMP wc_end JEQ

        ; not sep and not newline
        g_is_inside_word LOAD wc_loop JNE
        GETRV 1 ADD SETRV
        1 g_is_inside_word SAVE
        wc_loop JMP
    :wc_end
        DROP
        RET


:main
    wc CALL
    0 HALT
