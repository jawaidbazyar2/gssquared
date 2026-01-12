
#include "debugger/trace.hpp"

int main(int argc, char **argv) {
    char *tmsg;

    processor_type cputype = PROCESSOR_6502;
    const char *trace_filename = nullptr;
    
    if (argc < 3) {
        printf("usage: gstrace 6502|65c02|65816e tracefilename.bin\n");
        return 1;
    }
    
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "6502") == 0) {
            cputype = PROCESSOR_6502;
        } else if (strcmp(argv[i], "65c02") == 0) {
            cputype = PROCESSOR_65C02;
        } else if (strcmp(argv[i], "65816e") == 0) {
            cputype = PROCESSOR_65816;
        } else {
            printf("usage: gstrace 6502|65c02|65816e tracefilename.bin\n");
            return 1;
        }
    }
    
    trace_filename = argv[argc - 1];

    system_trace_buffer trace_buffer(100000, cputype);
    trace_buffer.read_from_file(trace_filename);
    
    for (size_t i = 0; i < trace_buffer.size; i++) {
        system_trace_entry_t *entry = trace_buffer.get_entry(i);
        tmsg = trace_buffer.decode_trace_entry(entry);
        printf("%s\n", tmsg);
    }
}
