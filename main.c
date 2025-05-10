#include "c-flags/single-header/c-flags.h"
#include <inttypes.h>

#define BIPCA_IMPLEMENTATION
#include "bipca.h"
#include "chemodan.h"

int main(int argc, char *argv[]) {
    if (argc > 0)
        c_flags_set_application_name(argv[0]);

    c_flags_set_positional_args_description("<file-path>...");
    c_flags_set_description("Model assembler interpreter");

    bool *isMemOverseerEnabled = c_flag_bool("memoverseer", "mo", "enable MemOverseer plugin", false);
    bool *isMemDumpEnabled = c_flag_bool("memorydump", "md", "enable MemoryDump plugin", false);
    bool *interpretStepByStep = c_flag_bool("stepbystep", "s", "enable step-by-step interpretation", false);
    bool *help = c_flag_bool("help", "h", "show this message", false);

    c_flags_parse(&argc, &argv, false);

    if (*help) {
        c_flags_usage();
        return 0;
    }

    if (argc == 0) {
        printf("ERROR: required file path not specified\n\n");
        c_flags_usage();
        return 1;
    }

    Error err;
    err = TranslateFromFiles(argc, argv);
    if (err) return 1;
    PrintProgram();
    if (*isMemOverseerEnabled) {
        err = AddPlugin(&MemOverseerPlugin);
        if (err) {
            fprintf(stderr, "plugin MemOverseer failed to initialize\n");
            return 1;
        }
    }
    if (*isMemDumpEnabled) {
        err = AddPlugin(&MemoryDumpPlugin);
        if (err) {
            fprintf(stderr, "plugin MemDump failed to initialize\n");
            return 1;
        }
    }
    printf("%d\n", Interpret((InterpretParams) { .stepByStepInterpretation = *interpretStepByStep}));
    return 0;
}
