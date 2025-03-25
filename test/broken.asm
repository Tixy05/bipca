readnum CALL HALT

:g_cur_char -1

foo JMP
:getchar ; ... rv -> ... char
g_cur_char LOAD DUP ; ... rv [g_cur_char] [g_cur_char]
getchar_empty JLT
; опустошаем буфер
; ... rv [g_cur_char]  | [g_cur_char] >= 0
1 NEG g_cur_char SAVE
SWAP RET
:getchar_empty ; ... rv -1
DROP ; ... rv
IN   ; ... rv char
SWAP ; ... char rv
RET

1234567890976543213456 DUP DUP DUP
:booo
@
:ADD
:peekchar ; ... rv -> ... char
getchar CALL DUP g_cur_char SAVE SWAP RET
:readnum ; ... rv -> ... value
0 ; ... rv 0=accum
:readnum_loop ; ... rv accum
peekchar CALL ; ... rv accum char
48 CMP readnum_exit JLT
12 :booo 
peekchar CALL
57 CMP readnum_exit JGT
getchar CALL 48 SUB ; ... rv accum digit
SWAP 10 MUL ADD
readnum_loop JMP
:readnum_exit ; ... rv accum
SWAP RET