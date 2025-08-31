
#include "debugger/trace.hpp"

int main(int argc, char **argv) {
    char *tmsg;

    system_trace_buffer trace_buffer(100000);
    trace_buffer.read_from_file("trace.bin");
    
    for (size_t i = 0; i < trace_buffer.size; i++) {
        system_trace_entry_t *entry = trace_buffer.get_entry(i);
        tmsg = trace_buffer.decode_trace_entry(entry);
        printf("%s\n", tmsg);
    }
}
