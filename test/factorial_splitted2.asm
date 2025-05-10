        DUP 1 CMP exit JLE
        DUP GETRV MUL SETRV
        1 SUB
        loop JMP
    :exit
        DROP
        RET