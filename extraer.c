#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (256 * 1024)
#define MAX_FILENAME_LEN 255

typedef struct {
    char filename[MAX_FILENAME_LEN + 1];
    long file_size;
} FileEntry;

void extract_files_from_tar(const char *tar_filename) {
    FILE *tar_file = fopen(tar_filename, "rb");
    if (!tar_file) {
        printf("Unable to open TAR file %s\n", tar_filename);
        return;
    }

    // Read number of files
    int num_files;
    fread(&num_files, sizeof(int), 1, tar_file);

    // Extract each file from TAR
    for (int i = 0; i < num_files; i++) {
        FileEntry entry;
        fread(&entry, sizeof(FileEntry), 1, tar_file);

        FILE *file = fopen(entry.filename, "wb");
        if (!file) {
            printf("Unable to create file %s\n", entry.filename);
            continue;
        }

        // Read file contents block by block
        char buffer[BLOCK_SIZE];
        size_t bytes_to_read = entry.file_size;
        while (bytes_to_read > 0) {
            size_t bytes_read = fread(buffer, 1, bytes_to_read < BLOCK_SIZE ? bytes_to_read : BLOCK_SIZE, tar_file);
            fwrite(buffer, 1, bytes_read, file);
            bytes_to_read -= bytes_read;
        }

        fclose(file);
    }

    fclose(tar_file);
}

int main() {
    extract_files_from_tar("prueba-paq.tar");
    printf("Extraction complete.\n");
    return 0;
}
