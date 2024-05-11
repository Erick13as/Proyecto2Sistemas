#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

// Función obtner el nombre del archivo
char* processFileOption(int argc, char *argv[]) {
    char *archive_name = NULL;
    int i;

    int next_arg_index = i + 1;
    while (next_arg_index < argc && argv[next_arg_index][0] == '-') {
        next_arg_index++;
    }
    if (next_arg_index < argc) {
        char *potential_name = argv[next_arg_index];
        int name_length = strlen(potential_name);
        if (name_length >= 4 && strcmp(potential_name + name_length - 4, ".tar") == 0) {
            archive_name = argv[next_arg_index];
        } else {
            printf("El nombre del archivo empacado debe terminar con \".tar\".\n");
            return NULL;
        }
    } else {
        printf("Uso: %s -f <archivo_empacado> [archivos]\n", argv[0]);
        return NULL;
    }

    return archive_name;
}

// Función para crear un nuevo archivo empacado
void pack_files_to_tar(const char *tar_filename, char *filenames[], int num_files, int verbose) {
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

// Función para extraer los archivos existentes dentro del archivo empacado
void extract_files_from_tar(const char *tar_filename, int verbose) {
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

// Función para actualizar un archivo existente dentro del archivo empacado ---(Genera fallos en los datos de los archivos)---
void update_file_in_tar(const char *tar_filename, const char *filename, int verbose) {
    FILE *tar_file = fopen(tar_filename, "r+b");
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

    // Find the entry for the file to be updated
    int file_index = -1;
    for (int i = 0; i < num_files; i++) {
        if (strcmp(file_entries[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }

    if (file_index == -1) {
        printf("File %s not found in TAR file\n", filename);
        fclose(tar_file);
        return;
    }

    // Open the new file for reading
    FILE *new_file = fopen(filename, "rb");
    if (!new_file) {
        printf("Unable to open file %s\n", filename);
        fclose(tar_file);
        return;
    }

    // Calculate new file size
    fseek(new_file, 0L, SEEK_END);
    long new_file_size = ftell(new_file);
    rewind(new_file);

    // Calculate the size difference
    long size_difference = new_file_size - file_entries[file_index].file_size;

    // Update subsequent file offsets if needed
    for (int i = file_index + 1; i < num_files; i++) {
        file_entries[i].offset += size_difference;
    }

    // Move subsequent file content if needed
    if (size_difference != 0) {
        // Determine the position from which to read and write
        long read_position = ftell(tar_file) + file_entries[file_index].offset + file_entries[file_index].file_size;
        long write_position = ftell(tar_file) + file_entries[file_index].offset + new_file_size;

        // Calculate the number of bytes to move
        long remaining_bytes = ftell(tar_file) - read_position;
        char buffer[BLOCK_SIZE];
        while (remaining_bytes > 0) {
            // Determine the number of bytes to read in this iteration
            size_t bytes_to_read = remaining_bytes < BLOCK_SIZE ? remaining_bytes : BLOCK_SIZE;

            // Read data from the source position
            fseek(tar_file, read_position, SEEK_SET);
            fread(buffer, 1, bytes_to_read, tar_file);

            // Write data to the target position
            fseek(tar_file, write_position, SEEK_SET);
            fwrite(buffer, 1, bytes_to_read, tar_file);

            // Update positions and remaining bytes
            read_position += bytes_to_read;
            write_position += bytes_to_read;
            remaining_bytes -= bytes_to_read;
        }
    }

    // Write the new file content to the TAR file
    fseek(tar_file, file_entries[file_index].offset, SEEK_SET);
    long remaining_size = new_file_size;
    for (int i = 0; i < file_entries[file_index].num_blocks; i++) {
        char buffer[BLOCK_SIZE];
        size_t bytes_to_read = remaining_size < BLOCK_SIZE ? remaining_size : BLOCK_SIZE;
        fread(buffer, 1, bytes_to_read, new_file);
        fwrite(buffer, 1, bytes_to_read, tar_file);
        remaining_size -= bytes_to_read;
    }

    // Update file size in the file entry
    file_entries[file_index].file_size = new_file_size;

    // Update file entries in the TAR file
    rewind(tar_file);
    fwrite(&num_files, sizeof(int), 1, tar_file);
    fwrite(file_entries, sizeof(FileEntry), num_files, tar_file);

    // Close files
    fclose(new_file);
    fclose(tar_file);

    printf("File %s updated successfully\n", filename);
}

// Función para listar los nombres de los archivos dentro del archivo empacado 
void list_files_in_tar(const char *tar_filename, int verbose) {
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

    // Print the names of all files
    printf("Files in TAR archive:\n");
    for (int i = 0; i < num_files; i++) {
        printf("%s\n", file_entries[i].filename);
    }

    // Close the TAR file
    fclose(tar_file);
}

// Función para agregar nuevos archivos al archivo empacado ---(Las demas funciones no detectan el archivo)---
void add_file_to_tar(const char *tar_filename, const char *filename, int verbose) {
    FILE *tar_file = fopen(tar_filename, "a+b");
    if (!tar_file) {
        printf("Unable to open TAR file %s\n", tar_filename);
        return;
    }

    // Open the file to be added
    FILE *file_to_add = fopen(filename, "rb");
    if (!file_to_add) {
        printf("Unable to open file %s\n", filename);
        fclose(tar_file);
        return;
    }

    // Calculate file size
    fseek(file_to_add, 0L, SEEK_END);
    long file_size = ftell(file_to_add);
    rewind(file_to_add);

    // Calculate number of blocks needed
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Write file content to TAR archive
    fseek(tar_file, 0, SEEK_END);
    char buffer[BLOCK_SIZE];
    for (int i = 0; i < num_blocks; i++) {
        size_t bytes_to_read = (i == num_blocks - 1) ? file_size % BLOCK_SIZE : BLOCK_SIZE;
        fread(buffer, 1, bytes_to_read, file_to_add);
        fwrite(buffer, 1, bytes_to_read, tar_file);
    }

    // Close files
    fclose(file_to_add);
    fclose(tar_file);

    printf("File %s added to TAR archive\n", filename);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s [-v] [--verbose] [-f <archivo_empacado>] [--file <archivo_empacado>] <opciones> [archivos]\n", argv[0]);
        return 1;
    }

    char *archive_name = NULL;
    char **files_to_use = NULL;
    int num_files = 0;
    int verbose = 0;

    // Procesar opciones antes de llamar a la función correspondiente
    int i;
    for (i = 1; i < argc; i++) {
        char *option = argv[i];

        if (option[0] == '-') {
            if (option[1] == '-') {
                // Forma completa de la opción
                printf("%s \n", option);
                if (strcmp(option, "--verbose") == 0) {
                    verbose++;
                } else if (strcmp(option, "--file") == 0) {
                    archive_name = processFileOption(argc, argv);
                    if (archive_name == NULL) {
                        return 1;
                    }
                }
            } else {
                // Forma abreviada de la opción
                for (int j = 1; option[j] != '\0'; j++) {
                    char opt = option[j];
                    printf("%c \n", opt);

                    switch (opt) {
                        case 'v':
                            verbose++;
                            break;
                        case 'f':
                            archive_name = processFileOption(argc, argv);
                            if (archive_name == NULL) {
                                return 1;
                            }
                            break;
                    }
                }
            }
        }
    }

    //Archivos a usar
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], archive_name) == 0) {
            int j = i + 1;
            while (j < argc && argv[j][0] != '-') {
                j++;
            }
            num_files = j - i - 1;
            files_to_use = &argv[i + 1];
        }
    }

    // Llamar a la función correspondiente después de procesar todas las opciones
    for (i = 1; i < argc; i++) {
        
        char *option = argv[i];

        if (option[0] == '-') {
            if (option[1] == '-') {
                // Forma completa de la opción
                if (strcmp(option, "--create") == 0) {
                    pack_files_to_tar(archive_name, files_to_use, num_files, verbose);
                } else if (strcmp(option, "--update") == 0) {
                    update_file_in_tar(archive_name, files_to_use[0], verbose);
                }else if (strcmp(option, "--list") == 0) {
                    list_files_in_tar(archive_name, verbose);
                }else if (strcmp(option, "--append") == 0) {
                    add_file_to_tar(archive_name, files_to_use[0], verbose);
                }else if (strcmp(option, "--extract") == 0) {
                    extract_files_from_tar(archive_name, verbose);
                }
                
            } else {
                // Forma abreviada de la opción
                for (int j = 1; option[j] != '\0'; j++) {
                    char opt = option[j];

                    switch (opt) {
                        case 'c':
                            pack_files_to_tar(archive_name, files_to_use, num_files, verbose);
                            break;
                        case 'u':
                            update_file_in_tar(archive_name, files_to_use[0], verbose);
                            break;
                        case 't':
                            list_files_in_tar(archive_name, verbose);
                            break;
                        case 'r':
                            add_file_to_tar(archive_name, files_to_use[0], verbose);
                            break;
                        case 'x':
                            extract_files_from_tar(archive_name, verbose);
                            break;
                    }
                }
            }
        }
    }

    return 0;
}

//gcc star8.c -o star
//./star -cvf prueba-paq.tar prueba.txt
//./star --create --verbose --file prueba-paq.tar prueba.txt
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx
//./star -uvf prueba-paq.tar prueba.txt
//./star -tvf prueba-paq.tar
//./star -rvf prueba-paq.tar prueba2.docx
//./star -xvf prueba-paq.tar
