5 fact CALL
0 HALT

:fact
    ; ... num retaddr
    1 SETRV
    SWAP ; ... retaddr num
    :loop