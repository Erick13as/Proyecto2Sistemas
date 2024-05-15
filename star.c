#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE (256 * 1024) 
#define MAX_FILES 100 
#define MAX_FILENAME_LENGTH 256
#define MAX_BLOCKS_PER_FILE 64 
#define MAX_BLOCKS MAX_BLOCKS_PER_FILE * MAX_FILES

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    long file_size;
    long block_positions[MAX_BLOCKS_PER_FILE];
    long num_blocks; 
} FileEntry;

typedef struct {
    FileEntry files[MAX_FILES];
    long num_files;
    long free_blocks[MAX_BLOCKS];
    long num_free_blocks;
} FAT;

typedef struct {
    unsigned char data[BLOCK_SIZE];
} Block; 

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

void pack_files_to_tar(const char *tar_filename, char **filenames, int num_files, int verbose) {
    if (verbose == 1) printf("Creando archivo %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando a crear el archivo %s\n", tar_filename);

    FILE *archive = fopen(tar_filename, "wb"); // Abrir archivo como binario para escritura

    if (archive == NULL) {
        fprintf(stderr, "Error al abrir el archivo %s\n", tar_filename);
        exit(1);
    }

    FAT fat;
    memset(&fat, 0, sizeof(FAT)); // Inicializar FAT con 0s

    fat.free_blocks[0] = sizeof(FAT); // El primer bloque libre está después de la FAT
    fat.num_free_blocks = 1; // Solo hay un bloque libre

    fwrite(&fat, sizeof(FAT), 1, archive); // Escribir la FAT en el archivo (posición 0)

    for (int i = 0; i < num_files; i++) {
        FILE *input_file = fopen(filenames[i], "rb"); // Abrir archivo como binario para lectura
        if (input_file == NULL) {
            fprintf(stderr, "Error al abrir el archivo %s\n", filenames[i]);
            exit(1);
        }

        if (verbose >= 2) printf("Agregando archivo %s\n", filenames[i]);

        long file_size = 0;
        long block_count = 0;
        Block block;
        long bytes_read;

        while ((bytes_read = fread(&block, 1, sizeof(Block), input_file)) > 0) {
            // Mientras se pueda leer un bloque del archivo
            FAT * fat_point = &fat;
            long block_position = -1;
            for (long i = 0; i < fat_point->num_free_blocks; i++) {
                if (fat_point->free_blocks[i] != 0) {
                    long free_block = fat_point->free_blocks[i];
                    fat_point->free_blocks[i] = 0; 
                    block_position = free_block;
                }
            }
            if (block_position == -1) {
                // Si no hay bloques libres
                if (verbose >= 2) printf("No hay bloques libres, expandiendo el archivo\n");
                fseek(archive, 0, SEEK_END); 
                long current_size = ftell(archive); 
                long expanded_size = current_size + BLOCK_SIZE; 
                ftruncate(fileno(archive), expanded_size); 
                fat_point->free_blocks[fat_point->num_free_blocks++] = current_size; 
                for (long i = 0; i < fat_point->num_free_blocks; i++) {
                    if (fat_point->free_blocks[i] != 0) {
                        long free_block = fat_point->free_blocks[i];
                        fat_point->free_blocks[i] = 0; 
                        block_position = free_block;
                    }
                }
                if (verbose >= 2) printf("Nuevo bloque libre en la posición %zu\n", block_position);
            }

            if (bytes_read < sizeof(Block)) {
                // Si no se lee un bloque completo
                memset((char*)&block + bytes_read, 0, sizeof(Block) - bytes_read); // Rellenar con 0s
            }

            fseek(archive, block_position, SEEK_SET); 
            fwrite(&block, sizeof(Block), 1, archive); 
            int repeated = 0;
            for (long j = 0; j < fat_point->num_files; j++) { 
                if (strcmp(fat_point->files[j].filename, filenames[i]) == 0) { 
                    fat_point->files[j].block_positions[fat_point->files[j].num_blocks++] = block_position;  
                    fat_point->files[j].file_size += bytes_read; 
                    repeated++; 
                }
            }

            if (repeated == 0) {
                FileEntry new_entry;
                strncpy(new_entry.filename, filenames[i], MAX_FILENAME_LENGTH); 
                new_entry.file_size = file_size + bytes_read; 
                new_entry.block_positions[0] = block_position; 
                new_entry.num_blocks = 1; 
                fat_point->files[fat_point->num_files++] = new_entry; 
            }

            file_size += bytes_read;
            block_count++;

            if (verbose >= 2) {
                printf("Escribiendo bloque %zu para archivo %s\n", block_position, filenames[i]);
            }
        }

        if (verbose == 1 || verbose >= 2) printf("Tamaño del archivo %s: %zu bytes\n", filenames[i], file_size);

        fclose(input_file);
    }

    fseek(archive, 0, SEEK_SET); 
    fwrite(&fat, sizeof(FAT), 1, archive); 
    fclose(archive);

    if (verbose >= 2) {
        printf("Creación del archivo %s completada.\n", tar_filename);
    } else if (verbose == 1) {
        printf("Archivo %s creado.\n", tar_filename);
    }
}

void extract_files_from_tar(const char *tar_filename, int verbose) {
    if (verbose == 1) printf("Extrayendo archivos del archivo %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando a extraer archivos del archivo %s\n", tar_filename);

    // Abrir el archivo TAR en modo de lectura binaria
    FILE *tar_file = fopen(tar_filename, "rb");
    if (tar_file == NULL) {
        printf("Error al abrir el archivo TAR para lectura.\n");
        return;
    }

    // Leer la FAT del archivo TAR
    FAT fat;
    fread(&fat, sizeof(FAT), 1, tar_file);

    // Iterar sobre cada archivo en la FAT y extraerlo
    for (long i = 0; i < fat.num_files; i++) {
        FileEntry file_entry = fat.files[i];
        FILE *output_file = fopen(file_entry.filename, "wb");
        if (output_file == NULL) {
            printf("Error al crear el archivo de salida: %s\n", file_entry.filename);
            continue;
        }

        if (verbose >= 2) {
            printf("Extrayendo archivo: %s\n", file_entry.filename);
        }

        long file_size = 0;
        // Iterar sobre cada bloque del archivo y escribirlo en el archivo de salida
        for (long j = 0; j < file_entry.num_blocks; j++) {
            Block block;
            fseek(tar_file, file_entry.block_positions[j], SEEK_SET);
            fread(&block, sizeof(Block), 1, tar_file);

            long bytes_to_write = (file_size + sizeof(Block) > file_entry.file_size) ? file_entry.file_size - file_size : sizeof(Block);
            fwrite(&block, 1, bytes_to_write, output_file);

            file_size += bytes_to_write;
        }

        fclose(output_file);

        if (verbose >= 2) {
            printf("Extracción del archivo %s completada.\n", file_entry.filename);
        }
    }

    // Cerrar el archivo TAR
    fclose(tar_file);

    if (verbose >= 2) {
        printf("Extracción de archivos completada.\n");
    } else if (verbose == 1) {
        printf("Archivos extraídos del archivo %s.\n", tar_filename);
    }
}

void list_files_in_tar(const char *tar_filename, int verbose) {
    if (verbose == 1) printf("Listando archivos en el archivo %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando a listar archivos en el archivo %s\n", tar_filename);

    // Abrir el archivo TAR en modo de lectura binaria
    FILE *tar_file = fopen(tar_filename, "rb");
    if (tar_file == NULL) {
        printf("Error al abrir el archivo TAR para lectura.\n");
        return;
    }

    // Leer la FAT del archivo TAR
    FAT fat;
    fread(&fat, sizeof(FAT), 1, tar_file);

    // Iterar sobre cada archivo en la FAT y mostrar su información
    for (long i = 0; i < fat.num_files; i++) {
        FileEntry file_entry = fat.files[i];
        printf("Nombre: %s, Tamaño: %zu bytes\n", file_entry.filename, file_entry.file_size);
    }

    // Cerrar el archivo TAR
    fclose(tar_file);

    if (verbose >= 2) {
        printf("Listado de archivos completado.\n");
    } else if (verbose == 1) {
        printf("Archivos listados en el archivo %s.\n", tar_filename);
    }
}

void add_file_to_tar(const char *tar_filename, char **filenames, int num_files, int verbose) {
    if (verbose == 1) printf("Añadiendo archivos al archivo %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando a añadir archivos al archivo %s\n", tar_filename);

    // Abrir el archivo TAR en modo de lectura y escritura binaria
    FILE *tar_file = fopen(tar_filename, "r+b");
    if (tar_file == NULL) {
        printf("Error al abrir el archivo TAR para lectura y escritura.\n");
        return;
    }

    // Leer la FAT del archivo TAR
    FAT fat;
    fread(&fat, sizeof(FAT), 1, tar_file);

    // Buscar el último bloque ocupado en el archivo
    long last_block = 0;
    for (long i = 0; i < fat.num_files; i++) {
        FileEntry file_entry = fat.files[i];
        for (long j = 0; j < file_entry.num_blocks; j++) {
            if (file_entry.block_positions[j] > last_block) {
                last_block = file_entry.block_positions[j];
            }
        }
    }

    // Iterar sobre los nuevos archivos y agregarlos al archivo TAR
    for (int i = 0; i < num_files; i++) {
        FILE *input_file = fopen(filenames[i], "rb"); // Abrir archivo como binario para lectura
        if (input_file == NULL) {
            fprintf(stderr, "Error al abrir el archivo %s\n", filenames[i]);
            continue;
        }

        if (verbose >= 2) printf("Agregando archivo %s\n", filenames[i]);
        long file_size = 0;
        long block_count = 0;
        Block block;
        long bytes_read;

        while ((bytes_read = fread(&block, 1, sizeof(Block), input_file)) > 0) {
            // Mientras se pueda leer un bloque del archivo
            FAT * fat_point = &fat;
            long block_position = -1;
            for (long i = 0; i < fat_point->num_free_blocks; i++) {
                if (fat_point->free_blocks[i] != 0) {
                    long free_block = fat_point->free_blocks[i];
                    fat_point->free_blocks[i] = 0; 
                    block_position = free_block;
                }
            }
            if (block_position == -1) {
                // Si no hay bloques libres
                if (verbose >= 2) printf("No hay bloques libres, expandiendo el archivo\n");
                fseek(tar_file, 0, SEEK_END); 
                long current_size = ftell(tar_file); 
                long expanded_size = current_size + BLOCK_SIZE; 
                ftruncate(fileno(tar_file), expanded_size); 
                fat_point->free_blocks[fat_point->num_free_blocks++] = current_size; 
                for (long i = 0; i < fat_point->num_free_blocks; i++) {
                    if (fat_point->free_blocks[i] != 0) {
                        long free_block = fat_point->free_blocks[i];
                        fat_point->free_blocks[i] = 0; 
                        block_position = free_block;
                    }
                }
                if (verbose >= 2) printf("Nuevo bloque libre en la posición %zu\n", block_position);
            }

            if (bytes_read < sizeof(Block)) {
                // Si no se lee un bloque completo
                memset((char*)&block + bytes_read, 0, sizeof(Block) - bytes_read); // Rellenar con 0s
            }

            fseek(tar_file, block_position, SEEK_SET); 
            fwrite(&block, sizeof(Block), 1, tar_file); 
            int repeated = 0;
            for (long j = 0; j < fat_point->num_files; j++) { 
                if (strcmp(fat_point->files[j].filename, filenames[i]) == 0) { 
                    fat_point->files[j].block_positions[fat_point->files[j].num_blocks++] = block_position;  
                    fat_point->files[j].file_size += bytes_read; 
                    repeated++; 
                }
            }

            if (repeated == 0) {
                FileEntry new_entry;
                strncpy(new_entry.filename, filenames[i], MAX_FILENAME_LENGTH); 
                new_entry.file_size = file_size + bytes_read; 
                new_entry.block_positions[0] = block_position; 
                new_entry.num_blocks = 1; 
                fat_point->files[fat_point->num_files++] = new_entry; 
            }

            file_size += bytes_read;
            block_count++;

            if (verbose >= 2) {
                printf("Escribiendo bloque %zu para archivo %s\n", block_position, filenames[i]);
            }
        }

        if (verbose >= 2) printf("Tamaño del archivo %s: %zu bytes\n", filenames[i], file_size);

        fclose(input_file);
    }

    fseek(tar_file, 0, SEEK_SET); 
    fwrite(&fat, sizeof(FAT), 1, tar_file); 
    fclose(tar_file);

    if (verbose >= 2) {
        printf("Añadido completado.\n");
    } else if (verbose == 1) {
        printf("Archivos añadidos al archivo %s.\n", tar_filename);
    }
}

void delete_from_tar(const char *tar_filename, char **filenames, int num_files, int verbose) {
    if (verbose == 1) printf("Eliminando archivos del archivo %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando a eliminar archivos del archivo %s\n", tar_filename);

    // Abrir el archivo TAR en modo de lectura y escritura binaria
    FILE *tar_file = fopen(tar_filename, "r+b");
    if (tar_file == NULL) {
        printf("Error al abrir el archivo TAR para lectura y escritura.\n");
        return;
    }

    // Leer la FAT del archivo TAR
    FAT fat;
    fread(&fat, sizeof(FAT), 1, tar_file);

    // Iterar sobre los archivos en filenames y eliminarlos del archivo TAR y de la FAT
    for (int i = 0; i < num_files; i++) {
        char *filename_to_delete = filenames[i];
        bool found = false;

        // Iterar sobre los archivos en la FAT y encontrar el archivo a eliminar
        for (long j = 0; j < fat.num_files; j++) {
            FileEntry *file_entry = &fat.files[j];
            if (strcmp(file_entry->filename, filename_to_delete) == 0) {
                found = true;

                // Eliminar bloques ocupados por el archivo de la lista de bloques libres
                for (long k = 0; k < file_entry->num_blocks; k++) {
                    long block_position = file_entry->block_positions[k];
                    fat.free_blocks[fat.num_free_blocks++] = block_position;
                }

                // Mover las entradas restantes de la FAT para cerrar el espacio
                for (long k = j; k < fat.num_files - 1; k++) {
                    fat.files[k] = fat.files[k + 1];
                }

                fat.num_files--;
                break;
            }
        }

        if (!found) {
            printf("El archivo %s no existe en el archivo TAR.\n", filename_to_delete);
        } else {
            if (verbose >= 2) {
                printf("Archivo %s eliminado del archivo TAR.\n", filename_to_delete);
            }
        }
    }

    // Escribir la FAT actualizada en el archivo TAR
    fseek(tar_file, 0, SEEK_SET);
    fwrite(&fat, sizeof(FAT), 1, tar_file);

    fclose(tar_file);

    if (verbose >= 2) {
        printf("Eliminación completada.\n");
    } else if (verbose == 1) {
        printf("Archivos eliminados del archivo %s.\n", tar_filename);
    }
}

void defragment_tar(const char *tar_filename, int verbose) {
    if (verbose == 1) printf("Desfragmentando el archivo %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando la desfragmentación del archivo %s\n", tar_filename);

    FILE *tar_file = fopen(tar_filename, "r+b");
    if (tar_file == NULL) {
        printf("Error al abrir el archivo TAR para lectura y escritura.\n");
        return;
    }

    FAT fat;
    fread(&fat, sizeof(FAT), 1, tar_file);

    long new_block_position = sizeof(FAT);
    for (long i = 0; i < fat.num_files; i++) {
        FileEntry *entry = &fat.files[i];
        long file_size = 0;

        for (long j = 0; j < entry->num_blocks; j++) {
            Block block;
            fseek(tar_file, entry->block_positions[j], SEEK_SET);
            fread(&block, sizeof(Block), 1, tar_file);

            fseek(tar_file, new_block_position, SEEK_SET);
            fwrite(&block, sizeof(Block), 1, tar_file);

            entry->block_positions[j] = new_block_position;
            new_block_position += sizeof(Block);
            file_size += sizeof(Block);

            if (verbose >= 2) {
                printf("Bloque %zu del archivo '%s' movido a la posición %zu\n", j + 1, entry->filename, entry->block_positions[j]);
            }
        }

        if (verbose >= 2) {
            printf("Archivo '%s' desfragmentado.\n", entry->filename);
        }
    }

    fat.num_free_blocks = 0;
    long remaining_space = new_block_position;
    while (remaining_space < fat.free_blocks[fat.num_free_blocks - 1]) {
        fat.free_blocks[fat.num_free_blocks++] = remaining_space;
        remaining_space += sizeof(Block);
    }

    fseek(tar_file, 0, SEEK_SET);
    fwrite(&fat, sizeof(FAT), 1, tar_file);

    ftruncate(fileno(tar_file), new_block_position);

    fclose(tar_file);

    if (verbose >= 2) {
        printf("Desfragmentación completada.\n");
    } else if (verbose == 1) {
        printf("Archivo %s desfragmentado.\n", tar_filename);
    }
}

void update_file_in_tar(const char *tar_filename, char **filenames, int num_files, int verbose) {
    if (verbose == 1) printf("Actualizando archivos en %s\n", tar_filename);
    else if (verbose >= 2) printf("Comenzando la actualización de archivos en %s\n", tar_filename);

    FILE *tar_file = fopen(tar_filename, "r+b");
    if (tar_file == NULL) {
        printf("Error al abrir el archivo TAR para lectura y escritura.\n");
        return;
    }

    FAT fat;
    fread(&fat, sizeof(FAT), 1, tar_file);

    for (int i = 0; i < num_files; i++) {
        char *filename_to_update = filenames[i];
        bool found = false;

        for (long j = 0; j < fat.num_files; j++) {
            FileEntry *file_entry = &fat.files[j];
            if (strcmp(file_entry->filename, filename_to_update) == 0) {
                found = true;

                for (long k = 0; k < file_entry->num_blocks; k++) {
                    long block_position = file_entry->block_positions[k];
                    fat.free_blocks[fat.num_free_blocks++] = block_position;
                }

                FILE *input_file = fopen(filename_to_update, "rb");
                if (input_file == NULL) {
                    fprintf(stderr, "Error al abrir el archivo %s\n", filename_to_update);
                    continue;
                }

                long file_size = 0;
                long block_count = 0;
                Block block;
                long bytes_read;

                while ((bytes_read = fread(&block, 1, sizeof(Block), input_file)) > 0) {
                    FAT * fat_point = &fat;
                    long block_position = -1;
                    for (long i = 0; i < fat_point->num_free_blocks; i++) {
                        if (fat_point->free_blocks[i] != 0) {
                            long free_block = fat_point->free_blocks[i];
                            fat_point->free_blocks[i] = 0; 
                            block_position = free_block;
                        }
                    }
                    if (block_position == -1) {
                        if (verbose >= 2) printf("No hay bloques libres, expandiendo el archivo\n");
                        fseek(tar_file, 0, SEEK_END); 
                        long current_size = ftell(tar_file); 
                        long expanded_size = current_size + BLOCK_SIZE; 
                        ftruncate(fileno(tar_file), expanded_size); 
                        fat_point->free_blocks[fat_point->num_free_blocks++] = current_size; 
                        for (long i = 0; i < fat_point->num_free_blocks; i++) {
                            if (fat_point->free_blocks[i] != 0) {
                                long free_block = fat_point->free_blocks[i];
                                fat_point->free_blocks[i] = 0; 
                                block_position = free_block;
                            }
                        }
                        if (verbose >= 2) printf("Nuevo bloque libre en la posición %zu\n", block_position);
                    }

                    if (bytes_read < sizeof(Block)) {
                        memset((char*)&block + bytes_read, 0, sizeof(Block) - bytes_read);
                    }

                    fseek(tar_file, block_position, SEEK_SET); 
                    fwrite(&block, sizeof(Block), 1, tar_file); 
                    int repeated = 0;
                    for (long k = 0; k < fat_point->num_files; k++) { 
                        if (strcmp(fat_point->files[k].filename, filename_to_update) == 0) { 
                            fat_point->files[k].block_positions[fat_point->files[k].num_blocks++] = block_position;  
                            fat_point->files[k].file_size += bytes_read; 
                            repeated++; 
                        }
                    }

                    if (repeated == 0) {
                        FileEntry new_entry;
                        strncpy(new_entry.filename, filename_to_update, MAX_FILENAME_LENGTH); 
                        new_entry.file_size = file_size + bytes_read; 
                        new_entry.block_positions[0] = block_position; 
                        new_entry.num_blocks = 1; 
                        fat_point->files[fat_point->num_files++] = new_entry; 
                    }

                    file_size += bytes_read;
                    block_count++;

                    if (verbose >= 2) {
                        printf("Escribiendo bloque %zu para archivo %s\n", block_position, filename_to_update);
                    }
                }

                if (verbose >= 2) printf("Tamaño del archivo %s: %zu bytes\n", filename_to_update, file_size);

                fclose(input_file);
                break;
            }
        }

        if (!found) {
            printf("El archivo %s no existe en el archivo TAR.\n", filename_to_update);
        } else {
            if (verbose == 1) printf("Archivo %s actualizado en el archivo TAR.\n", filename_to_update);
            else if (verbose >= 2) printf("Actualizado archivo %s en el archivo TAR.\n", filename_to_update);
        }
    }

    fseek(tar_file, 0, SEEK_SET);
    fwrite(&fat, sizeof(FAT), 1, tar_file);

    fclose(tar_file);

    if (verbose >= 2) {
        printf("Actualización completada.\n");
    } else if (verbose == 1) {
        printf("Archivos actualizados en %s.\n", tar_filename);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: ./star <opciones> <archivoSalida> <archivo1> <archivo2> ... <archivoN>\n");
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

    if (archive_name == NULL) {
        printf("No se proporcionó el nombre del archivo TAR a utilizar.\n");
        return 1;
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
                    return 0;
                } else if (strcmp(option, "--update") == 0) {
                    update_file_in_tar(archive_name, files_to_use, num_files, verbose);
                    return 0;
                }else if (strcmp(option, "--list") == 0) {
                    list_files_in_tar(archive_name, verbose);
                    return 0;
                }else if (strcmp(option, "--append") == 0) {
                    add_file_to_tar(archive_name, files_to_use, num_files, verbose);
                    return 0;
                }else if (strcmp(option, "--extract") == 0) {
                    extract_files_from_tar(archive_name, verbose);
                    return 0;
                }else if (strcmp(option, "--delete") == 0) {
                    delete_from_tar(archive_name, files_to_use, num_files, verbose);
                    return 0;
                }else if (strcmp(option, "--pack") == 0) {
                    defragment_tar(archive_name, verbose);
                    return 0;
                }
                
            } else {
                // Forma abreviada de la opción
                for (int j = 1; option[j] != '\0'; j++) {
                    char opt = option[j];

                    switch (opt) {
                        case 'c':
                            pack_files_to_tar(archive_name, files_to_use, num_files, verbose);
                            return 0;
                        case 'u':
                            update_file_in_tar(archive_name, files_to_use, num_files, verbose);
                            return 0;
                        case 't':
                            list_files_in_tar(archive_name, verbose);
                            return 0;
                        case 'r':
                            add_file_to_tar(archive_name, files_to_use, num_files, verbose);
                            return 0;
                        case 'x':
                            extract_files_from_tar(archive_name, verbose);
                            return 0;
                        case 'p':
                            defragment_tar(archive_name, verbose);
                            return 0;
                    }
                }
            }
        }
    }

    return 0;
}

//gcc star.c -o star

//---Pruebas---

//---Crear tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//Ejemplo con palabras completas
//./star --create --verbose --file prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//Ejemplo con vv
//./star -cvvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf

//---Extraer tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//./star -xvf prueba-paq.tar

//---Listar contenido del tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//./star -tvf prueba-paq.tar

//---Agregar archivo al tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx
//./star -rvf prueba-paq.tar prueba3.pdf

//---Actualizar algun archivo del tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//./star -uvf prueba-paq.tar prueba.txt

//---Borrar algun archivo del tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//./star --delete -vf prueba-paq.tar prueba2.docx

//---Desfragmentar tar---
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx prueba3.pdf
//./star --delete -vf prueba-paq.tar prueba2.docx
//./star -pvf prueba-paq.tar
