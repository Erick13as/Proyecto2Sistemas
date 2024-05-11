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

void pack_files_to_tar(const char *tar_filename, const char *filenames[], int num_files) {
    FILE *tar_file = fopen(tar_filename, "wb");
    if (!tar_file) {
        printf("Unable to create TAR file %s\n", tar_filename);
        return;
    }

    // Create an array to store file entries
    FileEntry file_entries[MAX_FILES];

    // Write number of files
    fwrite(&num_files, sizeof(int), 1, tar_file);

    // Write dummy file entries (placeholders)
    fwrite(file_entries, sizeof(FileEntry), num_files, tar_file);

    // Write each file to TAR
    long current_offset = ftell(tar_file);
    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(filenames[i], "rb");
        if (!file) {
            printf("Unable to open file %s\n", filenames[i]);
            continue;
        }

        // Write filename and size
        strcpy(file_entries[i].filename, filenames[i]);
        fseek(file, 0L, SEEK_END);
        file_entries[i].file_size = ftell(file);
        rewind(file);

        // Write file contents block by block
        char buffer[BLOCK_SIZE];
        size_t bytes_read;
        file_entries[i].num_blocks = 0;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            fwrite(buffer, 1, bytes_read, tar_file);
            file_entries[i].block_positions[file_entries[i].num_blocks++] = current_offset;
            current_offset += bytes_read;
        }

        // Save file offset
        file_entries[i].offset = ftell(tar_file) - file_entries[i].file_size;

        fclose(file);
    }

    // Go back to the beginning and write file entries again
    fseek(tar_file, sizeof(int), SEEK_SET);
    fwrite(file_entries, sizeof(FileEntry), num_files, tar_file);

    fclose(tar_file);
}

int main() {
    const char *filenames[] = {"prueba.txt", "prueba2.docx", "prueba3.pdf"};
    pack_files_to_tar("prueba-paq.tar", filenames, sizeof(filenames) / sizeof(filenames[0]));
    printf("Packing complete.\n");
    return 0;
}
