
#include "debugger/trace.hpp"

int main(int argc, char **argv) {
    char *tmsg;

    processor_type cputype = PROCESSOR_6502;
    const char *trace_filename = nullptr;
    const char *label_filename = nullptr;
    
    if (argc < 3) {
        printf("usage: gstrace [options] 6502|65c02|65816e tracefilename.bin\n");
        printf("options:\n");
        printf("  -l labelfile    VICE label file\n");
        return 1;
    }
    
    // Parse arguments
    int i = 1;
    while (i < argc - 1) {
        if (strcmp(argv[i], "-l") == 0) {
            if (i + 1 >= argc - 1) {
                printf("Error: -l requires a filename\n");
                return 1;
            }
            label_filename = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "6502") == 0) {
            cputype = PROCESSOR_6502;
            i++;
        } else if (strcmp(argv[i], "65c02") == 0) {
            cputype = PROCESSOR_65C02;
            i++;
        } else if (strcmp(argv[i], "65816e") == 0) {
            cputype = PROCESSOR_65816;
            i++;
        } else {
            printf("usage: gstrace [options] 6502|65c02|65816e tracefilename.bin\n");
            printf("options:\n");
            printf("  -l labelfile    VICE label file\n");
            return 1;
        }
    }
    
    trace_filename = argv[argc - 1];

    system_trace_buffer trace_buffer(100000, cputype);
    
    // Load labels if specified
    if (label_filename != nullptr) {
        trace_buffer.load_labels_from_file(label_filename);
    }
    
    trace_buffer.read_from_file(trace_filename);
    
    for (size_t i = 0; i < trace_buffer.size; i++) {
        system_trace_entry_t *entry = trace_buffer.get_entry(i);
        tmsg = trace_buffer.decode_trace_entry(entry);
        printf("%s\n", tmsg);
    }
}
