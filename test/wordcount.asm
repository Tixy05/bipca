; sep = {',', '.', ';', '?', '!', '\t', ' '} = {44, 46, 59, 63, 33, 9, 32}

main JMP

:g_is_inside_word 0

:wc
    0 SETRV
    :loop
    IN ; ... c
    DUP 44 CMP sep JEQ
    DUP 46 CMP sep JEQ
    DUP 59 CMP sep JEQ
    DUP 63 CMP sep JEQ
    DUP 33 CMP sep JEQ
    DUP 32 CMP sep JEQ
    DUP 9 CMP sep JEQ

    DUP 10 CMP end JEQ
    DUP 13 CMP end JEQ

    g_is_inside_word LOAD do_not_inc_rv JNE ; ... c -> ... 
    g_is_inside_word 1 SAVE
    GETRV 1 ADD SETRV
:do_not_inc_rv
    DROP
    loop JMP
:sep
    g_is_inside_word 0 SAVE
    DROP
    loop JMP
:end
    DROP
    RET

:main
    wc CALL
    0 HALT
