#pragma once

#include <cstdio>
#include <cstdarg>

/**
 * A class to write 65816 assembly code to a file.
 */
class AsmFile {
    private:
        FILE *f;
    public:
        AsmFile(const char *filename) : f(fopen(filename, "w")) {}
        ~AsmFile() {fclose(f);}
        void w(const char *format, ...) {
            va_list args;
            va_start(args, format);
            vfprintf(f, format, args);
            va_end(args);
            fprintf(f, "\n");
        }
};