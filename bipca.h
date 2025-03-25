// BIPCA - "BIPCA" Interpreter for Programs in Custom Assembler
#include <stdint.h>
#include <malloc.h>
#include <strings.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define DEBUG 1
#define LOG_DEBUG(fmt, ...) \
    do { if (DEBUG) fprintf(stdout, "[DEBUG]: " fmt, ##__VA_ARGS__); } while (0)
#define LOG_INFO(fmt, ...) fprintf(stdout, "[INFO]: " fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR]: " fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) fprintf(stdout, "[WARNING]: " fmt, ##__VA_ARGS__)

#define TEXT_BOLD_RED(text)   "\033[1;31m" text "\033[0m"
#define TEXT_BOLD_CYAN(text)  "\033[1;36m" text "\033[0m"
#define TEXT_BOLD_GREEN(text) "\033[1;32m" text "\033[0m"
#define TEXT_BOLD(text)       "\033[1m" text "\033[0m"

#define SIZE (2 << 20) // min 10^6; 1 MiB
#define PROGRAM_TEXT_SIZE (2 << 23) // 8 MiB
#define RESERVED 256

#define N_MAX_PLUGINS 64
#define PLUGIN_NAME_MAX_LENGTH (64 - 1)

#define WORD_SIZE 32
typedef int32_t Word;

#define UNDEF 0xDEADBEEF 
#define MAX_IDENT_LENGTH (64 - 1) // one char reserved for '\0'
#define MAX_N_IDENT (2 << 14)
// max memory for storing idents is 64 * (2 << 14) = 1 MiB

#define MAX_FILENAME_LENGTH (256 - 1)
#define MAX_N_FILES 256 

Word M[SIZE] = {0};

typedef enum {
    ADD    = -1,
    SUB    = -2,
    MUL    = -40,
    DIV    = -41,
    MOD    = -42,
    NEG    = -33,

    BITAND = -3,
    BITOR  = -4,
    BITXOR = -5,
    BITNOT = -34,
    LSHIFT = -6,
    RSHIFT = -7,

    DUP    = -25,
    DROP   = -26,
    SWAP   = -27,
    ROT    = -28,
    OVER   = -29,
    SDROP  = -30,
    DROP2  = -24,

    LOAD   = -35,
    SAVE   = -36,

    GETIP  = -9,
    GETSP  = -10,
    GETFP  = -11,
    GETRV  = -12,
    SETIP  = -13,
    SETSP  = -14,
    SETFP  = -15,
    SETRV  = -16,

    CMP    = -8,
    JMP    = -13,
    JLT    = -23,
    JGT    = -20,
    JEQ    = -22,
    JLE    = -21,
    JGE    = -18,
    JNE    = -19,

    CALL   = -31,
    RET    = -13,
    RET2   = -17,

    IN     = -43,
    OUT    = -44,
    HALT   = -37,
} Command;

typedef enum {
    NO_ERROR,

    ERR_UNKNOWN_IDENT,
    ERR_TOO_MANY_IDENTS,
    ERR_IDENT_TOO_LONG,

    ERR_PROGRAM_TOO_LONG,
    ERR_FILENAME_TOO_LONG,

    ERR_UNEXPECTED_CHARACTER,

    ERR_EMPTY_LABEL,
    ERR_LABEL_REDEFINITION,
    ERR_KEYWORD_REDEFINITION,

    ERR_CANT_READ_FILE,

    ERR_NUMBER_TOO_BIG,

    ERR_TOO_MANY_PLUGINS,
} Error;

typedef struct {
    size_t row;
    size_t col;
} Position;  

typedef struct {
    Position pos;
    size_t filenameIndex;
} Coord;

char files[MAX_FILENAME_LENGTH + 1][MAX_N_FILES] = {0};

Coord coords[SIZE] = {0};

typedef struct {
    Word address;
    bool isUserDefined;
    Position position;
} IdentInfo;

struct {
    struct {
        char key[MAX_IDENT_LENGTH + 1];
        IdentInfo value;
        bool occupied;
    } table[MAX_N_IDENT];
} identMap = {0};

void PrintInstructionCoords(Word instructionIndex) {
    Coord c = coords[instructionIndex];
    printf(TEXT_BOLD("%s:%zu:%zu: "), files[c.filenameIndex], c.pos.row + 1, c.pos.col + 1);
}

size_t _Hash(char* str) {
    size_t hash = 5381;
    int c;
    while ((c = (int) *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % MAX_N_IDENT;
}

Error NewIdent(const char* key, IdentInfo value) {
    size_t idx = _Hash((char*)key);
    size_t original_idx = idx;
    
    while (identMap.table[idx].occupied) {
        if (strcmp(identMap.table[idx].key, key) == 0) {
            identMap.table[idx].value = value;
            return NO_ERROR;
        }
        idx = (idx + 1) % MAX_N_IDENT;
        if (idx == original_idx) {
            return ERR_TOO_MANY_IDENTS;
        }
    }
    strncpy(identMap.table[idx].key, key, MAX_IDENT_LENGTH);
    identMap.table[idx].key[MAX_IDENT_LENGTH] = '\0';
    identMap.table[idx].value = value;
    identMap.table[idx].occupied = 1;
    return NO_ERROR;
}

// to just check for the occurence pass NULL as a value
bool GetIdent(const char* key, IdentInfo* value) {
    size_t idx = _Hash((char*) key);
    size_t original_idx = idx;
    
    while (identMap.table[idx].occupied) {
        if (strcmp(identMap.table[idx].key, key) == 0) {
            if (value != NULL) *value = identMap.table[idx].value;
            return true;
        }
        idx = (idx + 1) % MAX_N_IDENT;
        if (idx == original_idx) {
            break;
        }
    }
    return false; 
}

#define ADD_KEYWORD_IDENT(cmd) \
    NewIdent(#cmd, (IdentInfo) { \
        .address = cmd, \
        .isUserDefined = false \
        }); \

void InitIdentMap(void) {
    ADD_KEYWORD_IDENT(ADD);
    ADD_KEYWORD_IDENT(ADD);
    ADD_KEYWORD_IDENT(SUB);
    ADD_KEYWORD_IDENT(MUL);
    ADD_KEYWORD_IDENT(DIV);
    ADD_KEYWORD_IDENT(MOD);
    ADD_KEYWORD_IDENT(NEG);
    ADD_KEYWORD_IDENT(BITAND);
    ADD_KEYWORD_IDENT(BITOR);
    ADD_KEYWORD_IDENT(BITXOR);
    ADD_KEYWORD_IDENT(BITNOT);
    ADD_KEYWORD_IDENT(LSHIFT);
    ADD_KEYWORD_IDENT(RSHIFT);
    ADD_KEYWORD_IDENT(DUP);
    ADD_KEYWORD_IDENT(DROP);
    ADD_KEYWORD_IDENT(SWAP);
    ADD_KEYWORD_IDENT(ROT);
    ADD_KEYWORD_IDENT(OVER);
    ADD_KEYWORD_IDENT(SDROP);
    ADD_KEYWORD_IDENT(DROP2);
    ADD_KEYWORD_IDENT(LOAD);
    ADD_KEYWORD_IDENT(SAVE);
    ADD_KEYWORD_IDENT(GETIP);
    ADD_KEYWORD_IDENT(GETSP);
    ADD_KEYWORD_IDENT(GETFP);
    ADD_KEYWORD_IDENT(GETRV);
    ADD_KEYWORD_IDENT(SETIP);
    ADD_KEYWORD_IDENT(SETSP);
    ADD_KEYWORD_IDENT(SETFP);
    ADD_KEYWORD_IDENT(SETRV);
    ADD_KEYWORD_IDENT(CMP);
    ADD_KEYWORD_IDENT(JMP);
    ADD_KEYWORD_IDENT(JLT);
    ADD_KEYWORD_IDENT(JGT);
    ADD_KEYWORD_IDENT(JEQ);
    ADD_KEYWORD_IDENT(JLE);
    ADD_KEYWORD_IDENT(JGE);
    ADD_KEYWORD_IDENT(JNE);
    ADD_KEYWORD_IDENT(CALL);
    ADD_KEYWORD_IDENT(RET);
    ADD_KEYWORD_IDENT(RET2);
    ADD_KEYWORD_IDENT(IN);
    ADD_KEYWORD_IDENT(OUT);
    ADD_KEYWORD_IDENT(HALT);
}

struct {
    char fileName[MAX_FILENAME_LENGTH + 1];
    char text[PROGRAM_TEXT_SIZE];
    size_t size;
    size_t observed;
    Position position;
} program = {0};

void ResetPosition(void) {
    program.position = (Position) {0, 0};
    program.observed = 0;
}

Error ReadProgram(const char *filename) {
    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        return ERR_FILENAME_TOO_LONG;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        return ERR_CANT_READ_FILE;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    if (fileSize > PROGRAM_TEXT_SIZE) {
        fclose(file);
        return ERR_PROGRAM_TOO_LONG;
    }

    program.size = fread(program.text, 1, fileSize, file);
    fclose(file);

    strncpy(program.fileName, filename, MAX_FILENAME_LENGTH);
    program.fileName[MAX_FILENAME_LENGTH] = '\0';

    program.observed = 0;
    program.position.row = 0;
    program.position.col = 0;

    return NO_ERROR;
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsLetter(char c) { 
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool IsAlphaNumeric(char c) { return IsLetter(c) || IsDigit(c); }
bool IsWhitespace(char c) { return c == '\t' || c == '\n' || c == ' '; }
bool IsAllowedChar(char c) {
    return IsAlphaNumeric(c)
           || IsWhitespace(c)
           || c == ':'
           || c == '+'
           || c == '-'
           || c == '_'
           || c == ';';
}
char CurrentChar() { return program.text[program.observed]; }

void SkipUnnecessary() {
    bool IndexChanged = true; 
    while (program.observed < program.size && IndexChanged) {
        IndexChanged = false;
        // skip whitespaces
        while (
            program.observed < program.size
            && IsWhitespace(CurrentChar())
        ) {
            if (CurrentChar() == '\n') {
                program.position.row++;
                program.position.col = 0;
            } else {
                program.position.col++;
            }
            IndexChanged = true;
            program.observed++;
        }

        // skip comments
        if (CurrentChar() == ';') {
            while (
                program.observed < program.size
                &&
                CurrentChar() != '\n'
            ) {
                program.observed++;
            }
            program.observed++;
            program.position.row++;
            program.position.col = 0;
            IndexChanged = true;
        }
    }
}

void ShowIdents(void) {
    for (size_t i = 0; i < MAX_N_IDENT; i++) {
        if (identMap.table[i].occupied) {
            printf("Ident: %s\n", identMap.table[i].key);
            printf(
                "%s\n",
                identMap.table[i].value.isUserDefined ? "User defined" : "Keyword");
            printf(
                "Defined at %s:%zu:%zu\n", 
                program.fileName,
                identMap.table[i].value.position.row + 1,
                identMap.table[i].value.position.col + 1
            );
            printf("Addr/Value %d\n", identMap.table[i].value.address);
        }
    }
}

void _PrintLocationAndError() {
    fprintf(stderr, TEXT_BOLD("%s:%zu:%zu: "), 
            program.fileName, program.position.row + 1, program.position.col + 1);
    fprintf(stderr, TEXT_BOLD_RED("error: "));
}

void _PrintError(void) {
    fprintf(stderr, TEXT_BOLD_RED("error: "));
}


void ReportError(Error err) {
    // When error occured position may point to the: 
    // 1) first whitespace after the lexem;
    // 2) somewhere inside the lexem.
    // abcdef
    // ^
    // abcdef
    //       ^
    // abcdef
    //      ^
    // abcdef
    //    ^

    size_t wordStart = program.observed;
    size_t wordEnd = program.observed;
    // seek left to start 
    if (IsWhitespace(program.text[program.observed])) {
        wordStart--;
    }
    while (!IsWhitespace(program.text[wordStart])) {
        wordStart--;
    }
    wordStart++;

    // seek to the right or decrement right pos
    if (IsWhitespace(program.text[program.observed])) {
        wordEnd--;
    }
    while (!IsWhitespace(program.text[wordEnd])) {
        wordEnd++;
    }
    wordEnd--;

    size_t start = program.observed - program.position.col;
    size_t end = program.observed;
    while (
        end < program.size &&
        program.text[end] != '\n'
    ) {
        end++;
    }
    // GENERAL STRUCTURE
    // 1. if you want to specify the place where error occured
    // use _PrintLocationAndError(), otherwise _PrintError()
    //
    // 2. fprintf to stderr some message
    //
    // 3. if you want to include line and place where error occured
    // use break, otherwise return
    switch (err) {
    case ERR_TOO_MANY_PLUGINS:
        _PrintError();
        fprintf(stderr, "too many plugins (limit is %d)\n", N_MAX_PLUGINS);
        break;
    case ERR_UNEXPECTED_CHARACTER:
        _PrintLocationAndError();
        fprintf(stderr, "unexpected character\n");
        break;
    case ERR_CANT_READ_FILE:
        _PrintError();
        fprintf(stderr, "unable to read file\n");
        return;
    case ERR_EMPTY_LABEL:
        _PrintLocationAndError();
        fprintf(stderr, "empty label\n");
        break;
    case ERR_PROGRAM_TOO_LONG:
        _PrintError();
        fprintf(stderr, "file size is too big (limit is %d KiB)\n", PROGRAM_TEXT_SIZE/1024);
        break;
    case ERR_FILENAME_TOO_LONG:
        _PrintError();
        fprintf(stderr, "filename is too long (limit is %d)\n", MAX_FILENAME_LENGTH);
        break;
    case ERR_IDENT_TOO_LONG:
        _PrintLocationAndError();
        fprintf(stderr, "idedntifier too long (limit is %d)\n", MAX_IDENT_LENGTH);
        break;
    case ERR_KEYWORD_REDEFINITION:
        _PrintLocationAndError();
        fprintf(stderr, "keyword redefinition\n");
        break;
    case ERR_LABEL_REDEFINITION:
        _PrintLocationAndError();
        fprintf(stderr, "label redefinition\n");
        break;
    case ERR_TOO_MANY_IDENTS:
        _PrintError();
        fprintf(stderr, "too many idents\n");
        break;
    case ERR_UNKNOWN_IDENT:
        _PrintLocationAndError();
        fprintf(stderr, "ident used but never defined\n");
        break;
    case ERR_NUMBER_TOO_BIG:
        _PrintLocationAndError();
        fprintf(stderr, "number constant exceeds 32-bit limit (%d)\n", INT32_MAX);
        break;
    default:
        _PrintError();
        fprintf(stderr, "error\n");
        break;
    }
    fprintf(stderr, "%5zu | %.*s\n", program.position.row + 1, (int) (end - start), program.text + start);
    fprintf(stderr, "      | " "%*s", (int) (program.position.col - (program.observed - wordStart)), "");
    fprintf(stderr, TEXT_BOLD_RED("^"));
    for (size_t i = 0; i < wordEnd - wordStart; i++) fprintf(stderr, TEXT_BOLD_RED("~"));
    fprintf(stderr, "\n");
}

Word current = RESERVED;
Word oldCurrent = RESERVED;

void RestoreCurrentAfterFirstPass() {
    current = oldCurrent;
}

Error ParseIdent(char* identBuffer) {
    if (!(program.observed < program.size) || IsWhitespace(CurrentChar())) {
        return ERR_EMPTY_LABEL;
    }
    if (!IsAllowedChar(CurrentChar())) {
        return ERR_UNEXPECTED_CHARACTER;
    }
    if (!IsLetter(CurrentChar()) && !(CurrentChar() == '_')) {
        return ERR_UNEXPECTED_CHARACTER;
    }
    size_t idx = 0;
    identBuffer[idx] = CurrentChar();
    idx++;
    program.observed++;
    program.position.col++;
    while (
        program.observed < program.size
        &&
        idx < MAX_IDENT_LENGTH + 1
        &&
        (IsAlphaNumeric(CurrentChar())
        || CurrentChar() == '-'
        || CurrentChar() == '_')
    ) {
        identBuffer[idx] = CurrentChar();
        idx++;
        program.observed++;
        program.position.col++;
    } 
    if (idx == MAX_IDENT_LENGTH + 1) return ERR_IDENT_TOO_LONG;
    identBuffer[idx] = '\0';
    // LOG_DEBUG("%s\n", identBuffer);
    return NO_ERROR;
}

// void NextWhitespace() {}

Error FirstPass(void) {
    char ident[MAX_IDENT_LENGTH + 1];
    Error err = NO_ERROR;
    IdentInfo identInfo = {0};
    while (program.observed < program.size) {
        SkipUnnecessary();

        // label
        if (CurrentChar() == ':') {
            Position identPos = program.position;
            program.observed++;
            program.position.col++;
            err = ParseIdent(ident);
            if (err) return err;
            if (GetIdent(ident, &identInfo)) {
                return identInfo.isUserDefined 
                       ? ERR_LABEL_REDEFINITION
                       : ERR_KEYWORD_REDEFINITION;
            }

            NewIdent(ident, (IdentInfo) {
                .isUserDefined = true,
                .address = current,
                .position = identPos,
            });
            // LOG_INFO("%d\n", program.observed);
            continue;
        }

        // number
        if (
            CurrentChar() == '-'
            || CurrentChar() == '+'
            || IsDigit(CurrentChar())
        ) {
            current++;
            program.observed++;
            program.position.col++;
            if (CurrentChar() == '-') current++;
            while (program.observed < program.size && IsDigit(CurrentChar())) {
                program.observed++;
                program.position.col++;
            }
            if (!(program.observed == program.size) && !IsWhitespace(CurrentChar())) {
                return ERR_UNEXPECTED_CHARACTER;
            }
            continue;
        }

        // ident
        if (
            IsLetter(CurrentChar())
            || CurrentChar() == '_'
        ) {
            // LOG_DEBUG("ident parse\n");
            current++;
            err = ParseIdent(ident);
            if (err) return err;
            if (!(program.observed == program.size) && !IsWhitespace(CurrentChar())) {
                return ERR_UNEXPECTED_CHARACTER;
            }
            // LOG_DEBUG("ident parse end %s\n", ident);
            continue;
        }

        return ERR_UNEXPECTED_CHARACTER;
    }

    err = NewIdent("PROGRAM_SIZE", (IdentInfo) {.address = current, .isUserDefined = false});
    if (err) return err;
    return NO_ERROR;
}

bool GetProgramSize(Word* ProgramSize) {
    IdentInfo ii;
    bool found = GetIdent("PROGRAM_SIZE", &ii);
    if (!found) return true;
    *ProgramSize = ii.address;
    return false; 
}

Error SecondPass(void) {
    char ident[MAX_IDENT_LENGTH + 1];
    Error err = NO_ERROR;
    IdentInfo identInfo = {0};
    // printf("cur %d\n", current);
    while (program.observed < program.size) {
        // printf("cur  %d\n", current);
        // LOG_DEBUG("pass step\n");
        SkipUnnecessary();
        Position startPos = program.position;
        // label
        if (CurrentChar() == ':') {
            while (program.observed < program.size && !IsWhitespace(CurrentChar())) {
                program.observed++;
                program.position.col++;
            }
            continue;
        }

        // number
        if (
            CurrentChar() == '-'
            || CurrentChar() == '+'
            || IsDigit(CurrentChar())
        ) {
            bool isNeg = CurrentChar() == '-';
            Word number = 0;

            if (CurrentChar() == '-' || CurrentChar() == '+') {
                program.observed++;
                program.position.col++;
            }
            
            while (program.observed < program.size && IsDigit(CurrentChar())) {
                int digit = CurrentChar() - '0';
                if (number > (INT32_MAX - digit) / 10) {
                    return ERR_NUMBER_TOO_BIG;
                }
                number = 10 * number + digit;
                program.observed++;
                program.position.col++;
            }

            M[current] = isNeg ? -number : number;
            coords[current] = (Coord) {.pos = startPos};
            strncpy(files[coords[current].filenameIndex], program.fileName, MAX_FILENAME_LENGTH + 1);
            current++;

            if (!(program.observed == program.size) && !IsWhitespace(CurrentChar())) {
                return ERR_UNEXPECTED_CHARACTER;
            }
            continue;
        }

        // ident
        if (
            IsLetter(CurrentChar())
            || CurrentChar() == '_'
        ) {
            err = ParseIdent(ident);
            if (err) return err;
            bool found = GetIdent(ident, &identInfo);
            if (!found) {
                return ERR_UNKNOWN_IDENT;
            }
            M[current] = identInfo.address;
            coords[current] = (Coord) {.pos = startPos};
            strncpy(files[coords[current].filenameIndex], program.fileName, MAX_FILENAME_LENGTH + 1);   
            current++;
            if (!(program.observed == program.size) && !IsWhitespace(CurrentChar())) {
                return ERR_UNEXPECTED_CHARACTER;
            }
            continue;
        }

        return ERR_UNEXPECTED_CHARACTER;
    }

    return NO_ERROR;
}

bool TranslateProgram(void) {
    bool errOccured = false;
    Error err = NO_ERROR;
    do {
        err = FirstPass();
        if (err) {
            errOccured = true;
            ReportError(err);
            while (program.observed < program.size && !IsWhitespace(CurrentChar())) {
                program.observed++;
                program.position.col++;
            }
            if (program.observed == program.size) break;            
        }
    } while (err);
    RestoreCurrentAfterFirstPass();
    ResetPosition();
    do {
        err = SecondPass();
        if (err) {
            errOccured = true;
            ReportError(err);
            while (program.observed < program.size && !IsWhitespace(CurrentChar())) {
                program.observed++;
                program.position.col++;
            }
            if (program.observed == program.size) break;
        }
    } while (err);

    return errOccured;
}

bool TranslateFromFile(char *filename) {
    InitIdentMap();
    Error err = ReadProgram(filename);
    if (err) {
        ReportError(err);
        return true;
    };
    return TranslateProgram();
}

bool TranslateFromFiles(int nFiles, char *filenames[]) {
    bool errOccured = false;
    InitIdentMap();
    for (int i = 0; i < nFiles; i++) {
        Error err = ReadProgram(filenames[i]);
        if (err) {
            ReportError(err);
            return true;
        };    
        errOccured = errOccured || TranslateProgram();
        oldCurrent = current;
    }
    return errOccured;
}

typedef struct {
    char name[PLUGIN_NAME_MAX_LENGTH + 1];
    bool (*InitPlugin)(void**);
    void (*BeforeExecution)(void*, Command);
    void (*AfterExecution)(void*, Command);
} Plugin;

struct {
    Plugin plugins[N_MAX_PLUGINS];
    size_t size;
    void* userDataPointers[N_MAX_PLUGINS];
} plugins = {0};

Error AddPlugin(Plugin* p) {
    if (plugins.size >= N_MAX_PLUGINS) return ERR_TOO_MANY_PLUGINS;
    plugins.plugins[plugins.size++] = *p; 
    return NO_ERROR;
}

bool PLUGIN_INIT_DUMMY(void** addr) {
    (void) addr;
    return false;
}

void PLUGIN_BEFORE_EXEC_DUMMY(void* addr, Command cmd) {
    (void) addr;
    (void) cmd;
}

void PLUGIN_AFTER_EXEC_DUMMY(void* addr, Command cmd) {
    (void) addr;
    (void) cmd;
}

struct {
    Word IP;
    Word SP;
    Word FP;
    Word RV;
} registers = {
    .IP = RESERVED,
    .SP = SIZE,
    .FP = UNDEF,
    .RV = UNDEF,
};

typedef struct {
    bool stepByStepInterpretation;
} InterpretParams;

Word Interpret(InterpretParams p) {
    Word x, y, z, v, a, c;
    Word returnValue;
    // plugins
    for (size_t i = 0; i < plugins.size; i++) {
        Plugin p = plugins.plugins[i];
        bool err = p.InitPlugin(plugins.userDataPointers + i);
        if (err) {
            _PrintError();
            printf("plugin \"%s\" falied to initialize\n", p.name);
            return -1;
        } else {
            LOG_DEBUG("plugin \"%s\" initialized\n", p.name);
        }
    }
    size_t step = 1;
    while (true) {
        Word cmd = M[registers.IP++];
        // plugins
        for (size_t i = 0; i < plugins.size; i++) {
            Plugin p = plugins.plugins[i];
            p.BeforeExecution(plugins.userDataPointers[i], cmd);
        }
        switch (cmd) {
        case ADD:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x + y;
            break;
        case SUB:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x - y;
            break;
        case MUL:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x * y;
            break;
        case DIV:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x / y;
            break;
        case MOD:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x % y;
            break;
        case NEG:
            M[registers.SP] = -M[registers.SP];
            break;
        case BITAND:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x & y;
            break;
        case BITOR:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x | y;
            break;
        case BITXOR:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x ^ y;
            break;
        case BITNOT:
            M[registers.SP] = ~M[registers.SP];
            break;
        case LSHIFT:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x << y;
            break;
        case RSHIFT:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x >> y;
            break;
        case DUP:
            x = M[registers.SP];
            M[--registers.SP] = x;
            break;
        case DROP:
            registers.SP++;
            break;
        case SWAP:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = y;
            M[--registers.SP] = x;
            break;
        case ROT:
            z = M[registers.SP++];
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = y;
            M[--registers.SP] = z;
            M[--registers.SP] = x;
            break;
        case OVER:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x;
            M[--registers.SP] = y;
            M[--registers.SP] = x;
            break;
        case SDROP:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = y;
            break;
        case DROP2:
            registers.SP++;
            registers.SP++;
            break;
        case LOAD:
            a = M[registers.SP++];
            M[--registers.SP] = M[a];
            break;
        case SAVE:
            v = M[registers.SP++];
            a = M[registers.SP++];
            M[a] = v;
            break;
        case GETIP:
            M[--registers.SP] = registers.IP;
            break;
        case GETSP:
            x = registers.SP;
            M[--registers.SP] = x;
            break;
        case GETFP:
            M[--registers.SP] = registers.FP;
            break;
        case GETRV:
            M[--registers.SP] = registers.RV;
            break;
        // case SETIP: === JMP
        //     break;
        case SETSP:
            a = M[registers.SP++];
            registers.SP = a;
            break;
        case SETFP:
            a = M[registers.SP++];
            registers.FP = a;
            break;
        case SETRV:
            a = M[registers.SP++];
            registers.RV = a;
            break;
        case CMP:
            y = M[registers.SP++];
            x = M[registers.SP++];
            M[--registers.SP] = x < y 
                                ? -1 
                                : (x > y ? 1 : 0);
            break;
        case JMP:
            a = M[registers.SP++];
            registers.IP = a;
            break;
        case JLT:
            a = M[registers.SP++];
            x = M[registers.SP++];
            if (x < 0) registers.IP = a;
            break;
        case JGT:
            a = M[registers.SP++];
            x = M[registers.SP++];
            if (x > 0) registers.IP = a;
            break;
        case JEQ:
            a = M[registers.SP++];
            x = M[registers.SP++];
            if (x == 0) registers.IP = a;
            break;
        case JLE:
            a = M[registers.SP++];
            x = M[registers.SP++];
            if (x <= 0) registers.IP = a;
            break;
        case JGE:
            a = M[registers.SP++];
            x = M[registers.SP++];
            if (x >= 0) registers.IP = a;
            break;
        case JNE:
            a = M[registers.SP++];
            x = M[registers.SP++];
            if (x != 0) registers.IP = a;
            break;
        case CALL:
            a = M[registers.SP++];
            M[--registers.SP] = registers.IP;
            registers.IP = a;
            break;
        // case RET: === JMP
        //     break;
        case RET2:
            a = M[registers.SP++];
            registers.SP++;
            registers.IP = a;
            break;
        case IN:
            M[--registers.SP] = (Word) getchar();
            break;
        case OUT:
            c = M[registers.SP++];
            putchar((int) c);
            break;
        case HALT:
            returnValue = M[registers.SP++];
            goto cleanup_and_return;
        default:
            if (cmd < 0) {
                _PrintError();
                printf("unknown instruction with code %d\n", cmd);
                returnValue = -1; // return something is better than nothing
                goto cleanup_and_return;
            } else {
                M[--registers.SP] = cmd;
            }
            break;
        }

        // plugins
        for (size_t i = 0; i < plugins.size; i++) {
            Plugin p = plugins.plugins[i];
            p.AfterExecution(plugins.userDataPointers[i], cmd);
        }
        if (p.stepByStepInterpretation) {
            printf("step %zu completed, press <Enter> to proceed", step);
            getchar();
        }
        step++;
    }

    cleanup_and_return:
    for (size_t i; i < plugins.size; i++) free(plugins.userDataPointers[i]);
    return returnValue;
}


///////////////////////////////////////////////////////////////////

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
    if (GetProgramSize(&od->progSize)) { return true; }
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