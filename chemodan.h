// CHEMODAN - "CHEMODAN" Helper Extensions for Model Assembler Needs

/*
## GENERAL INTERFACE FOR PLUGINS

To create a plugin one must define three functions:
1. `bool InitPlugin(void** userData)`
  Function that initializes needed data for plugin and writes it to
  *userData. If error occured returns true, returns false otherwise.
2. `void BeforeExecution(void* userData, Command cmd)`
  Function that runs Before interpretating instruction.
3. `void AfterExecution(void* userData, Command cmd)`
  Function that runs After interpretating instruction.

## SOME USEFUL NOTES

- All memory is words, `Word` type is an alias for `int32_t`;
- virtual machine memory size is `SIZE`;
- memory is an array of size `SIZE` and of type `Word` called `M`;
- `RESERVED` number of words at the start of `M` are reserved so program
   starts at `M[RESERVED]`. This words should be always equal 0;
- use `bool GetProgramSize(Word* ProgramSize)` to get program size, note
  that function could fail (returns true) but if translator works correctly
  it should not. **Note:** program size is not the <<actual>> size of a program,
  but a largest index such that M[index] is a part of translated program.
  So, `RESERVED` number of always-zero-words are de facto the part of the program; 
- registers are stored in the global anonymous struct called `registers`;
- `void PrintInstructionCoords(Word instructionIndex)` prints location of the
  instruction placed at M[instructionIndex] in format `file:row:col: `;
- to access the instruction that is about to be executed in `BeforeExecution()`
  check `M[registers.IP]` value;
- error messages mimics gcc style so a number of helpful macros and function like
  `PrintInstructionCoords()` mentioned above are provided.
*/

#include "bipca.h"

/////////////////////////
// a-la valgrind 
/////////////////////////

typedef struct {
    bool isDefined[SIZE];
    bool isDefIP;
    bool isDefSP;
    bool isDefFP;
    bool isDefRV;
    Word progSize;
} MemOverseerData;

bool InitMemOverseer(void** userData) { 
    MemOverseerData* od = (MemOverseerData*) calloc(1, sizeof(MemOverseerData));
    if (!od) { return true; }
    if (!GetProgramSize(&od->progSize)) { return true; }
    // od->isDefined = {false, false, ..., false} 'cause of calloc()
    *userData = (void*) od;
    return false;
}

bool CheckStackPop(MemOverseerData* od, int n) {
    Word from = registers.SP;
    Word to = registers.SP + n;
    bool underflowFlag = false;
    bool notDefinedFlag = false;
    for (Word i = from; i < to; i++) {
        if (i >= SIZE) {
            printf("WARNING: next instruction will cause stack underflow\n");
            underflowFlag = true;
        }
        if (!od->isDefined[i]) {
            printf("WARNING: next instuction operates with undefined stack element\n");
            notDefinedFlag = true;
        }
        if (underflowFlag || notDefinedFlag) {
            return true;
        }
    }

    return false;
}

void BeforeExecMemOverseer(void* userData, Command cmd) {
    MemOverseerData* od = (MemOverseerData*) userData;
    // check IP
    if (!(RESERVED <= registers.IP && registers.IP <= od->progSize)) {
        printf("WARNING: IP is out of range [RESERVED, PROGRAM_SIZE]\n");
        printf("    IP = %d\n", registers.IP);
        printf("    RESERVED = %d\n", RESERVED);
        printf("    PROGRAM_SIZE = %d\n", od->progSize);
    }
    // check SP
    if (!(od->progSize < registers.SP)) {
        printf("WARNING: stack overflow, SP <= PROGRAM_SIZE\n");
        printf("    SP = %d\n", registers.SP);
        printf("    PROGRAM_SIZE = %d\n", od->progSize);
    } else if (!(registers.SP <= SIZE)) {
        printf("WARNING: stack underflow, SP > SIZE\n");
        printf("    SP = %d\n", registers.SP);
        printf("    SIZE = %d\n", SIZE);
    }
    Word a;
    switch (cmd) {
        case ADD:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x + y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case SUB:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x - y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case MUL:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x * y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case DIV:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x / y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case MOD:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x % y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case NEG:
            CheckStackPop(od, 1);
            // M[registers.SP] = -M[registers.SP];
            break;
        case BITAND:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x & y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case BITOR:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x | y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case BITXOR:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x ^ y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case BITNOT:
            // M[registers.SP] = ~M[registers.SP];
            CheckStackPop(od, 1);
            break;
        case LSHIFT:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x << y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;    
            break;
        case RSHIFT:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x >> y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case DUP:
            // x = M[registers.SP];
            // M[--registers.SP] = x;
            CheckStackPop(od, 1);
            od->isDefined[registers.SP - 1] = true;
            break;
        case DROP:
            // registers.SP++;
            od->isDefined[registers.SP] = false;
            break;
        case SWAP:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = y;
            // M[--registers.SP] = x;
            CheckStackPop(od, 2);
            break;
        case ROT:
            // z = M[registers.SP++];
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = y;
            // M[--registers.SP] = z;
            // M[--registers.SP] = x;
            CheckStackPop(od, 3);
            break;
        case OVER:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x;
            // M[--registers.SP] = y;
            // M[--registers.SP] = x;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP - 1] = true;
            break;
        case SDROP:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = y;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case DROP2:
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            // registers.SP++;
            // registers.SP++;
            break;
        case LOAD:
            // a = M[registers.SP++];
            // M[--registers.SP] = M[a];
            CheckStackPop(od, 1);
            if (!od->isDefined[M[registers.SP]]) {
                printf("WARNING: loading variable from undefined element of stack\n");
            }
            break;
        case SAVE:
            // v = M[registers.SP++];
            // a = M[registers.SP++];
            // M[a] = v;
            CheckStackPop(od, 2);
            a = M[registers.SP + 1]; 
            if (a <= od->progSize) {
                printf("WARNING: saving word to program memory or reserved memory\n");
            } else if (a >= SIZE) {
                printf("ERROR: saving word outside of memory\n");
            }
            if (a < SIZE) od->isDefined[a] = true;
            break;
        case GETIP:
            // M[--registers.SP] = registers.IP;
            od->isDefined[registers.SP - 1] = true;
            break;
        case GETSP:
            // x = registers.SP;
            // M[--registers.SP] = x;
            od->isDefined[registers.SP - 1] = true;
            break;
        case GETFP:
            // M[--registers.SP] = registers.FP;
            if (!od->isDefFP) {
                printf("WARNING: trying to get FP value but FP is undefined\n");
            }
            od->isDefined[registers.SP - 1] = true;
            break;
        case GETRV:
            // M[--registers.SP] = registers.RV;
            if (!od->isDefRV) {
                printf("WARNING: trying to get RV value but RV is undefined\n");
            }
            od->isDefined[registers.SP - 1] = true;
            break;
        // case SETIP: === JMP
        //     break;
        case SETSP:
            // a = M[registers.SP++];
            // registers.SP = a;
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            break;
        case SETFP:
            // a = M[registers.SP++];
            // registers.FP = a;
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            od->isDefFP = true;
            break;
        case SETRV:
            // a = M[registers.SP++];
            // registers.RV = a;
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            od->isDefRV = true;
            break;
        case CMP:
            // y = M[registers.SP++];
            // x = M[registers.SP++];
            // M[--registers.SP] = x < y 
            //                     ? -1 
            //                     : (x > y ? 1 : 0);
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            break;
        case JMP:
            // a = M[registers.SP++];
            // registers.IP = a;
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            break;
        case JLT:
            // a = M[registers.SP++];
            // x = M[registers.SP++];
            // if (x < 0) registers.IP = a;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case JGT:
            // a = M[registers.SP++];
            // x = M[registers.SP++];
            // if (x > 0) registers.IP = a;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case JEQ:
            // a = M[registers.SP++];
            // x = M[registers.SP++];
            // if (x == 0) registers.IP = a;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case JLE:
            // a = M[registers.SP++];
            // x = M[registers.SP++];
            // if (x <= 0) registers.IP = a;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case JGE:
            // a = M[registers.SP++];
            // x = M[registers.SP++];
            // if (x >= 0) registers.IP = a;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case JNE:
            // a = M[registers.SP++];
            // x = M[registers.SP++];
            // if (x != 0) registers.IP = a;
            CheckStackPop(od, 2);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case CALL:
            // a = M[registers.SP++];
            // M[--registers.SP] = registers.IP;
            // registers.IP = a;
            CheckStackPop(od, 1);
            break;
        // case RET: === JMP
        //     break;
        case RET2:
            // a = M[registers.SP++];
            // registers.SP++;
            // registers.IP = a;
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            od->isDefined[registers.SP + 1] = false;
            break;
        case IN:
            // PrintRegisters();
            // printf("wtf\n");
            // M[--registers.SP] = (Word) getchar();
            od->isDefined[registers.SP - 1] = true;
            break;
        case OUT:
            // c = M[registers.SP++];
            // putchar((int) c);
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            break;
        case HALT:
            // return M[registers.SP++];
            CheckStackPop(od, 1);
            od->isDefined[registers.SP] = false;
            break;
        default:
            // if (cmd < 0) {
            //     _PrintError();
            //     printf("unknown instruction with code %d\n", cmd);
            //     return 1;
            // } else {
            //     M[--registers.SP] = cmd;
            // }
            od->isDefined[registers.SP - 1] = true;
            break;
    }
}


/////////////////////////
// a-la gdb debugger
/////////////////////////

void CheckReservedMemoryAndRegisters(void) {
    printf("-----------" TEXT_BOLD("REGISTERS") "------------\n");
    printf("IP = %08X  (%d)\n", registers.IP, registers.IP);
    printf("SP = %08X  (%d)\n", registers.SP, registers.SP);
    printf("FP = %08X  (%d)\n", registers.FP, registers.FP);
    printf("RV = %08X  (%d)\n", registers.RV, registers.RV);
    printf("--------------------------------\n");
    bool nonZeroFound = false;
    printf("-----" TEXT_BOLD("RESERVED MEMORY START") "------\n");
    printf("<  only non-zeros are printed  >\n");
    for (size_t i = 0; i < RESERVED; i++) {
        if (M[i] != 0) {
            nonZeroFound = true;
            printf("[%08lX] %8X  (%d) WARNING - NOT NULL VALUE!\n", i, M[i], M[i]);
        } 
    }
    if (!nonZeroFound) {
        printf("------reserved memory " TEXT_BOLD_GREEN("okay") "------\n");
    }
    printf("------" TEXT_BOLD("RESERVED MEMORY END") "-------\n");
}

void AfterExecMemoryDump(void* userData, Command cmd) {
    (void) userData;
    (void) cmd;
    CheckReservedMemoryAndRegisters();
    size_t minZerosWindow = 8;
    size_t i = RESERVED;
    size_t zeros_start = 0;
    size_t zeros_end = 0;
    printf("-------" TEXT_BOLD("MAIN MEMORY START") "--------\n");
    while (i < SIZE) {
        if (M[i] == 0) {
            zeros_start = i;
            while (i < SIZE && M[i] == 0) {
                i++;
            }
            zeros_end = i - 1;
            if (zeros_end - zeros_start + 1 > minZerosWindow) {
                printf("[%08lX] %8X  (%d)\n", zeros_start, M[zeros_start], M[zeros_start]);
                printf("...\n");
                printf("[%08lX] %8X  (%d)\n", zeros_end, M[zeros_end], M[zeros_end]);
            } else {
                for (size_t j = zeros_start; j < zeros_end + 1; j++) {
                    printf("[%08lX] %8X  (%d)\n", i, M[j], M[j]);
                }
            }
            
            continue;
        } else {
            printf("[%08lX] %8X  (%d)\n", i, M[i], M[i]);
            i++;
            continue;
        }
    }
    printf("--------" TEXT_BOLD("MAIN MEMORY END") "---------\n");
}

Plugin MemOverseerPlugin = (Plugin) {
    .name = "MemOverseer",
    .InitPlugin = InitMemOverseer,
    .BeforeExecution = BeforeExecMemOverseer,
    .AfterExecution = PLUGIN_AFTER_EXEC_DUMMY,
};

Plugin MemoryDumpPlugin = (Plugin) {
    .name = "MemoryDump",
    .InitPlugin = PLUGIN_INIT_DUMMY,
    .BeforeExecution = PLUGIN_BEFORE_EXEC_DUMMY,
    .AfterExecution = AfterExecMemoryDump,
};