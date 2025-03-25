GETRV DROP 5 fact CALL
0 HALT

:fact
    ; ... num retaddr
    1 SETRV
    SWAP ; ... retaddr num
    :loop
        DUP 1 CMP exit JLE
        DUP GETRV MUL SETRV
        1 SUB
        loop JMP
    :exit
        DROP
        RET