#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (256 * 1024)
#define MAX_FILENAME_LEN 255
#define MAX_FILES 100

typedef struct {
    char filename[MAX_FILENAME_LEN + 1];
    long file_size;
    long offset;
    int num_blocks;
    long block_positions[MAX_FILES];
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

    // Read file entries
    FileEntry file_entries[MAX_FILES];
    fread(file_entries, sizeof(FileEntry), num_files, tar_file);

    // Extract each file from TAR
    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(file_entries[i].filename, "wb");
        if (!file) {
            printf("Unable to create file %s\n", file_entries[i].filename);
            continue;
        }

        // Read each block of the file
        for (int j = 0; j < file_entries[i].num_blocks; j++) {
            fseek(tar_file, file_entries[i].block_positions[j], SEEK_SET);
            char buffer[BLOCK_SIZE];
            size_t bytes_to_read = (j == file_entries[i].num_blocks - 1) ? file_entries[i].file_size - (j * BLOCK_SIZE) : BLOCK_SIZE;
            fread(buffer, 1, bytes_to_read, tar_file);
            fwrite(buffer, 1, bytes_to_read, file);
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
