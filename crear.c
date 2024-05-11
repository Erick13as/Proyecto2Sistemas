#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (256 * 1024)
#define MAX_FILENAME_LEN 255

typedef struct {
    char filename[MAX_FILENAME_LEN + 1];
    long file_size;
} FileEntry;

void pack_files_to_tar(const char *tar_filename, const char *filenames[], int num_files) {
    FILE *tar_file = fopen(tar_filename, "wb");
    if (!tar_file) {
        printf("Unable to create TAR file %s\n", tar_filename);
        return;
    }

    // Write number of files
    fwrite(&num_files, sizeof(int), 1, tar_file);

    // Write each file to TAR
    for (int i = 0; i < num_files; i++) {
        FILE *file = fopen(filenames[i], "rb");
        if (!file) {
            printf("Unable to open file %s\n", filenames[i]);
            continue;
        }

        // Write filename and size
        FileEntry entry;
        strcpy(entry.filename, filenames[i]);
        fseek(file, 0L, SEEK_END);
        entry.file_size = ftell(file);
        rewind(file);
        fwrite(&entry, sizeof(FileEntry), 1, tar_file);

        // Write file contents block by block
        char buffer[BLOCK_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            fwrite(buffer, 1, bytes_read, tar_file);
        }

        fclose(file);
    }

    fclose(tar_file);
}

int main() {
    const char *filenames[] = {"prueba.txt", "prueba2.docx", "prueba3.pdf"};
    pack_files_to_tar("prueba-paq.tar", filenames, sizeof(filenames) / sizeof(filenames[0]));
    printf("Packing complete.\n");
    return 0;
}
