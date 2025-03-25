#define BIPCA_IMPLEMENTATION
#include "bipca.h"
// #include "chemodan.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    Error err;
    err = TranslateFromFile(argv[1]);
    if (err) return 1;
    // char ident[MAX_IDENT_LENGTH];
    // InitIdentMap();
    // Error err = FirstPass();
    // if (err) ReportError(err);
    // // printf("%zu %zu %zu\n", program.position.row, program.position.col, program.observed);
    // // printf("%d\n", err);
    // RestoreCurrentAfterFirstPass();
    // ResetPosition();
    // err = SecondPass();
    // if (err) ReportError(err);
    // printf("err is %d\n", err);
    // // ShowIdents();

    // printf("\n");
    IdentInfo ii;
    GetIdent("PROGRAM_SIZE", &ii);
    for (Word i = RESERVED; i < ii.address; i++) {
        printf("%3d %4d    %s:%zu:%zu\n", i, M[i], files[coords[i].filenameIndex], coords[i].pos.row + 1, coords[i].pos.col + 1);
    }
    printf("\n");
    err = AddPlugin(&MemOverseerPlugin);
    if (err) return 1;
    err = AddPlugin(&MemoryDumpPlugin);
    if (err) return 1;
    printf("\n\n%d\n\n", Interpret((InterpretParams) { .stepByStepInterpretation = 1}));
    return 0;
}

