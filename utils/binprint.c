#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    FILE *fp;
    unsigned char buffer[16];
    size_t bytes_read;
    long offset = 0;
    int apple_mode = 0;
    const char *filename = NULL;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            apple_mode = 1;
        } else if (filename == NULL) {
            filename = argv[i];
        } else {
            fprintf(stderr, "Usage: %s [-a] <filename>\n", argv[0]);
            fprintf(stderr, "  -a  Strip high bit (mask with 0x7F) for Apple II characters\n");
            return 1;
        }
    }

    if (filename == NULL) {
        fprintf(stderr, "Usage: %s [-a] <filename>\n", argv[0]);
        fprintf(stderr, "  -a  Strip high bit (mask with 0x7F) for Apple II characters\n");
        return 1;
    }

    fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    while ((bytes_read = fread(buffer, 1, 16, fp)) > 0) {
        printf("%06lX: ", offset);
        
        // Print hex values
        for (size_t i = 0; i < 16; i++) {
            if (i < bytes_read) {
                printf("%02X", buffer[i]);
            } else {
                printf("  ");
            }
            
            if ((i + 1) % 16 != 0) {
                printf(" ");
                if ((i + 1) % 4 == 0) {
                    printf(" ");
                }
            }
        }

        printf("  ");

        // Print ASCII representation
        for (size_t i = 0; i < bytes_read; i++) {
            unsigned char c = buffer[i];
            if (apple_mode) {
                c &= 0x7F;  // Strip high bit for Apple II characters
            }
            if (c >= 32 && c <= 126) {
                printf("%c ", c);
            } else {
                printf(". ");
            }
        }
        
        printf("\n");
        offset += 16;
    }

    fclose(fp);
    return 0;
}
