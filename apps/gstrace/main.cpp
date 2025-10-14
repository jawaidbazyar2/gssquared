
#include "debugger/trace.hpp"

int main(int argc, char **argv) {
    char *tmsg;

    processor_type cputype = PROCESSOR_6502;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "6502") == 0) {
            cputype = PROCESSOR_6502;
        } else if (strcmp(argv[i], "65c02") == 0) {
            cputype = PROCESSOR_65C02;
        } else if (strcmp(argv[i], "65816e") == 0) {
            cputype = PROCESSOR_65816;
        } else {
            printf("usage: gstrace [6502|65c02|65816e]\n");
        }
    }

    system_trace_buffer trace_buffer(100000, cputype);
    trace_buffer.read_from_file("trace.bin");
    
    for (size_t i = 0; i < trace_buffer.size; i++) {
        system_trace_entry_t *entry = trace_buffer.get_entry(i);
        tmsg = trace_buffer.decode_trace_entry(entry);
        printf("%s\n", tmsg);
    }
}
