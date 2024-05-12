#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
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
typedef struct {
    int num_files;
} FAT;

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
        // Skip extraction if the file is marked as deleted
        if (file_entries[i].filename[0] == '\0') {
            continue;
        }

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

    // Print the names of all files that are not marked as deleted
    printf("Files in TAR archive:\n");
    for (int i = 0; i < num_files; i++) {
        if (file_entries[i].filename[0] != '\0') {
            printf("%s\n", file_entries[i].filename);
        }
    }

    // Close the TAR file
    fclose(tar_file);
}

// Función para agregar un archivo al archivo TAR
void add_file_to_tar(const char *tar_filename, const char *filename, int verbose) {
    FILE *tar_file = fopen(tar_filename, "r+b");
    if (!tar_file) {
        printf("No se puede abrir el archivo TAR %s\n", tar_filename);
        return;
    }

    // Leer la cantidad de archivos eliminados
    int num_deleted_files;
    fread(&num_deleted_files, sizeof(int), 1, tar_file);

    // Leer la información de los archivos eliminados
    FileEntry *deleted_files = (FileEntry *)malloc(num_deleted_files * sizeof(FileEntry));
    fread(deleted_files, sizeof(FileEntry), num_deleted_files, tar_file);

    // Abrir el archivo a agregar
    FILE *file_to_add = fopen(filename, "rb");
    if (!file_to_add) {
        printf("No se puede abrir el archivo %s\n", filename);
        fclose(tar_file);
        free(deleted_files);
        return;
    }

    // Calcular el tamaño del archivo a agregar
    fseek(file_to_add, 0L, SEEK_END);
    long file_size = ftell(file_to_add);
    rewind(file_to_add);

    // Calcular el número de bloques necesarios
    int num_blocks = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Calcular el espacio ocupado por los archivos eliminados
    long deleted_files_space = num_deleted_files * sizeof(FileEntry);

    // Calcular el espacio disponible después de los archivos eliminados
    fseek(tar_file, 0, SEEK_END);
    long available_space = ftell(tar_file) - sizeof(int) - deleted_files_space;

    // Comprobar si hay suficiente espacio disponible
    if (available_space < file_size) {
        printf("No hay suficiente espacio disponible para agregar el archivo %s\n", filename);
        printf("Expande el archivo TAR %s\n", tar_filename);

        // Expandir el archivo TAR
        fseek(tar_file, 0, SEEK_END);
        long current_size = ftell(tar_file);
        long new_size = current_size + (file_size - available_space);
        fseek(tar_file, 0, SEEK_SET);
        ftruncate(fileno(tar_file), new_size);

        // Mover los datos existentes después del encabezado a la nueva posición
        char *buffer = (char *)malloc(current_size - sizeof(int));
        fread(buffer, 1, current_size - sizeof(int), tar_file);
        fseek(tar_file, sizeof(int), SEEK_SET);
        fwrite(buffer, 1, current_size - sizeof(int), tar_file);

        // Actualizar la posición del archivo
        fseek(tar_file, 0, SEEK_END);
        available_space = ftell(tar_file) - sizeof(int) - deleted_files_space;

        free(buffer);
    }

    // Escribir el contenido del archivo en el archivo TAR
    fseek(tar_file, 0, SEEK_END);
    char buffer[BLOCK_SIZE];
    for (int i = 0; i < num_blocks; i++) {
        size_t bytes_to_read = (i == num_blocks - 1) ? file_size % BLOCK_SIZE : BLOCK_SIZE;
        fread(buffer, 1, bytes_to_read, file_to_add);
        fwrite(buffer, 1, bytes_to_read, tar_file);
    }

    // Actualizar la información de archivos eliminados si el nuevo archivo se superpone con un archivo eliminado
    for (int i = 0; i < num_deleted_files; i++) {
        if (strcmp(filename, deleted_files[i].filename) == 0) {
            deleted_files[i] = deleted_files[num_deleted_files - 1];
            num_deleted_files--;
            break;
        }
    }

    // Actualizar la cantidad de archivos eliminados y escribir la información de los archivos eliminados
    fseek(tar_file, sizeof(int), SEEK_SET);
    fwrite(&num_deleted_files, sizeof(int), 1, tar_file);
    fwrite(deleted_files, sizeof(FileEntry), num_deleted_files, tar_file);

    // Liberar memoria y cerrar archivos
    fclose(file_to_add);
    fclose(tar_file);
    free(deleted_files);

    printf("Archivo %s agregado al archivo TAR %s\n", filename, tar_filename);
}

void delete_from_tar(const char *tar_filename, const char *filename, int verbose) {
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

    // Find the entry for the file to be deleted
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

    // Mark the space occupied by the deleted file as available
    file_entries[file_index].filename[0] = '\0'; // Marking the filename as empty

    // Update number of files in the TAR file
    // num_files--;

    // Write updated file entries
    fseek(tar_file, sizeof(int), SEEK_SET); // Move to the position where file entries are stored
    fwrite(file_entries, sizeof(FileEntry), num_files, tar_file);

    // Track the freed space
    long freed_offset = file_entries[file_index].offset;
    long freed_size = file_entries[file_index].file_size;
    int freed_blocks = file_entries[file_index].num_blocks;

    // Update free space list
    fseek(tar_file, 0, SEEK_END);
    long end_offset = ftell(tar_file);
    long new_offset = freed_offset;
    long new_size = freed_size;
    int new_blocks = freed_blocks;
    for (int i = 0; i < num_files; i++) {
        if (file_entries[i].offset > freed_offset) {
            file_entries[i].offset -= freed_size;
        }
        if (file_entries[i].offset + file_entries[i].file_size <= freed_offset) {
            new_offset = file_entries[i].offset + file_entries[i].file_size;
            new_size += file_entries[i].file_size;
            new_blocks += file_entries[i].num_blocks;
        }
    }
    file_entries[num_files].offset = new_offset;
    file_entries[num_files].file_size = new_size;
    file_entries[num_files].num_blocks = new_blocks;

    // Write updated file entries
    fseek(tar_file, sizeof(int) + sizeof(FileEntry) * num_files, SEEK_SET);
    fwrite(&file_entries[num_files], sizeof(FileEntry), 1, tar_file);

    fclose(tar_file);

    printf("File %s deleted from TAR archive\n", filename);
}

void packArchive(char *archive_name, int verbose) {
    // Abrir el archivo empacado en modo lectura y escritura
    FILE *archive = fopen(archive_name, "r+b");
    if (!archive) {
        printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Desfragmentando contenido del archivo empacado: %s\n", archive_name);
    }

    // Leer la cantidad de archivos del archivo empacado
    int num_files;
    fread(&num_files, sizeof(int), 1, archive);

    // Calcular el tamaño de la estructura FAT
    size_t fat_size = sizeof(int) + num_files * sizeof(FileEntry);

    // Reorganizar las entradas de archivo en el archivo empacado
    FileEntry file_entries[MAX_FILES];
    fread(file_entries, sizeof(FileEntry), num_files, archive);

    long current_offset = sizeof(int);
    for (int i = 0; i < num_files; i++) {
        if (file_entries[i].filename[0] != '\0') {
            fseek(archive, current_offset, SEEK_SET);
            fwrite(&file_entries[i], sizeof(FileEntry), 1, archive);
            current_offset += sizeof(FileEntry);
        }
    }

    // Truncar el archivo para eliminar los datos no utilizados
    int result = ftruncate(fileno(archive), current_offset);
    if (result == -1) {
        printf("Error al truncar el archivo.\n");
        return;
    }

    // Actualizar la cantidad de archivos en el archivo empacado
    fseek(archive, 0, SEEK_SET);
    fwrite(&num_files, sizeof(int), 1, archive);

    // Cerrar el archivo empacado
    fclose(archive);

    if (verbose) {
        printf("Desfragmentación completada.\n");
    }
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
                }else if (strcmp(option, "--delete") == 0) {
                    printf("Opcion delete %s, %s\n",argv[i + 3], archive_name);
                    delete_from_tar(archive_name, argv[i + 3], verbose);
                    break;
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
                        case 'p':
                            packArchive(archive_name, verbose);
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

//Pruebas Sofi
//./star -cvf prueba-paq.tar prueba2.docx prueba.txt
//./star --delete -vf prueba-paq.tar prueba2.docx
//./star -pvf prueba-paq.tar
//./star -xvf prueba-paq.tar