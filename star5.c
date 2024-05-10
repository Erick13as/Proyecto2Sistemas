#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#define BLOCK_SIZE (256 * 1024)

// Estructura para la tabla de asignación de archivos (FAT)
typedef struct {
    uint64_t block_offsets[BLOCK_SIZE / sizeof(uint64_t)]; // Offset de los bloques de datos
    int is_free[BLOCK_SIZE / sizeof(uint64_t)]; // Marca los bloques libres o ocupados
} FAT;

// Estructura para almacenar información sobre un archivo
typedef struct {
    char filename[256];
    uint64_t offset; // Offset del archivo dentro del archivo empacado
    uint64_t size; // Tamaño del archivo
} FileEntry;

// Función para crear un nuevo archivo empacado
void createArchive(char *archive_name, char **files_to_pack, int num_files, int verbose) {
    // Agregar extensión ".tar" al nombre del archivo empacado si no tiene una extensión
    char *extension = ".tar";
    if (strstr(archive_name, ".tar") == NULL) {
        strcat(archive_name, extension);
    }

    // Abrir el archivo empacado en modo escritura
    FILE *archive = fopen(archive_name, "wb");
    if (!archive) {
        printf("Error: No se pudo crear el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Creando archivo empacado: %s\n", archive_name);
    }

    // Crear la estructura FAT
    FAT fat;
    memset(&fat, 0, sizeof(FAT));

    // Escribir la estructura FAT al inicio del archivo empacado
    fwrite(&fat, sizeof(FAT), 1, archive);

    // Crear una entrada de archivo para cada archivo a empacar
    FileEntry file_entries[num_files];
    for (int i = 0; i < num_files; i++) {
        // Obtener información sobre el archivo (nombre, tamaño)
        char *filename = files_to_pack[i];
        FILE *file = fopen(filename, "rb");
        if (!file) {
            printf("Error: No se pudo abrir el archivo %s.\n", filename);
            continue;
        }
        fseek(file, 0, SEEK_END);
        uint64_t file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Escribir el contenido del archivo al archivo empacado
        uint64_t offset = ftell(archive);
        char buffer[BLOCK_SIZE];
        while (1) {
            size_t bytes_read = fread(buffer, 1, BLOCK_SIZE, file);
            if (bytes_read == 0) break;
            fwrite(buffer, 1, bytes_read, archive);
        }

        // Guardar la entrada de archivo en la estructura FAT
        strcpy(file_entries[i].filename, filename);
        file_entries[i].offset = offset;
        file_entries[i].size = file_size;

        fclose(file);

        if (verbose) {
            printf("Añadiendo al archivo empacado: %s, Tamaño: %lu bytes\n", filename, file_size);
        }
    }

    // Escribir las entradas de archivo al archivo empacado
    fseek(archive, sizeof(FAT), SEEK_SET);
    fwrite(file_entries, sizeof(FileEntry), num_files, archive);

    // Cerrar el archivo empacado
    fclose(archive);

    if (verbose) {
        printf("Archivo empacado creado con éxito: %s\n", archive_name);
    }
}

// Función para actualizar un archivo existente dentro del archivo empacado
void updateArchive(char *archive_name, char **files_to_update, int num_files, int verbose) {
    // Abrir el archivo empacado en modo lectura y escritura
    FILE *archive = fopen(archive_name, "r+b");
    if (!archive) {
        printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Actualizando archivo empacado: %s\n", archive_name);
    }

    // Leer el contenido del archivo tar y las entradas de archivo
    fseek(archive, 0, SEEK_END);
    uint64_t archive_size = ftell(archive);
    fseek(archive, sizeof(FAT), SEEK_SET);

    // Leer las entradas de archivo desde el archivo empacado
    FileEntry file_entries[num_files];
    fread(file_entries, sizeof(FileEntry), num_files, archive);

    // Buscar los archivos a actualizar en las entradas de archivo
    for (int i = 0; i < num_files; i++) {
        char *filename = files_to_update[i];
        int file_found = 0;

        // Buscar el archivo en las entradas de archivo por su nombre
        for (int j = 0; j < num_files; j++) {
            if (strcmp(file_entries[j].filename, filename) == 0) {
                // El archivo fue encontrado, actualizar su contenido
                file_found = 1;
                uint64_t offset = file_entries[j].offset;
                uint64_t size = file_entries[j].size;

                // Mover el puntero de lectura/escritura al inicio del archivo dentro del archivo empacado
                fseek(archive, offset, SEEK_SET);

                // Leer el contenido del archivo original
                char buffer[BLOCK_SIZE];
                FILE *updated_file = fopen(filename, "rb");
                if (!updated_file) {
                    printf("Error: No se pudo abrir el archivo %s.\n", filename);
                    continue;
                }
                while (1) {
                    size_t bytes_read = fread(buffer, 1, BLOCK_SIZE, updated_file);
                    if (bytes_read == 0) break;
                    fwrite(buffer, 1, bytes_read, archive);
                }
                fclose(updated_file);

                if (verbose) {
                    printf("Archivo actualizado en el archivo empacado: %s\n", filename);
                }

                break;
            }
        }

        if (!file_found) {
            printf("Error: El archivo %s no existe en el archivo empacado.\n", filename);
        }
    }

    // Cerrar el archivo empacado
    fclose(archive);

    if (verbose) {
        printf("Archivo empacado actualizado con éxito: %s\n", archive_name);
    }
}

// Función para listar los nombres de los archivos dentro del archivo empacado
void listArchiveContents(char *archive_name, int verbose) {
    // Abrir el archivo empacado en modo lectura
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Listando contenidos del archivo empacado: %s\n", archive_name);
    }

    // Leer la estructura FAT
    FAT fat;
    fread(&fat, sizeof(FAT), 1, archive);

    // Leer las entradas de archivo desde el archivo empacado y listar los nombres de los archivos
    FileEntry file_entries[BLOCK_SIZE / sizeof(FileEntry)];
    fread(file_entries, sizeof(FileEntry), BLOCK_SIZE / sizeof(FileEntry), archive);

    printf("Contenidos del archivo empacado:\n");
    for (int i = 0; i < BLOCK_SIZE / sizeof(FileEntry); i++) {
        if (file_entries[i].filename[0] != '\0') {
            printf("%s\n", file_entries[i].filename);
        } else {
            // Si encontramos una entrada de archivo vacía, no hay más archivos que listar
            break;
        }
    }

    // Cerrar el archivo empacado
    fclose(archive);
}

// Función para agregar nuevos archivos al archivo empacado
void appendArchive(char *archive_name, char **files_to_append, int num_files, int verbose) {
    // Abrir el archivo empacado en modo lectura y escritura
    FILE *archive = fopen(archive_name, "r+b");
    if (!archive) {
        printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Agregando nuevos archivos al archivo empacado: %s\n", archive_name);
    }

    // Leer el contenido del archivo tar y las entradas de archivo
    fseek(archive, 0, SEEK_END);
    uint64_t archive_size = ftell(archive);
    fseek(archive, sizeof(FAT), SEEK_SET);

    // Leer las entradas de archivo desde el archivo empacado
    FileEntry file_entries[BLOCK_SIZE / sizeof(FileEntry)];
    fread(file_entries, sizeof(FileEntry), BLOCK_SIZE / sizeof(FileEntry), archive);

    // Encontrar una entrada de archivo vacía para agregar los nuevos archivos
    int empty_entry_index = -1;
    for (int i = 0; i < BLOCK_SIZE / sizeof(FileEntry); i++) {
        if (file_entries[i].filename[0] == '\0') {
            empty_entry_index = i;
            break;
        }
    }

    // Si no se encuentra una entrada de archivo vacía, el archivo empacado está lleno
    if (empty_entry_index == -1) {
        printf("Error: No hay suficiente espacio para agregar nuevos archivos al archivo empacado.\n");
        fclose(archive);
        return;
    }

    // Para cada archivo a agregar
    for (int i = 0; i < num_files; i++) {
        char *new_file = files_to_append[i];

        // Obtener información sobre el nuevo archivo
        FILE *new_file_ptr = fopen(new_file, "rb");
        if (!new_file_ptr) {
            printf("Error: No se pudo abrir el nuevo archivo %s.\n", new_file);
            continue;
        }

        fseek(new_file_ptr, 0, SEEK_END);
        uint64_t new_file_size = ftell(new_file_ptr);
        fseek(new_file_ptr, 0, SEEK_SET);

        // Mover el puntero de lectura/escritura al final del archivo empacado
        fseek(archive, 0, SEEK_END);

        // Leer el contenido del nuevo archivo y escribirlo al archivo empacado
        char buffer[BLOCK_SIZE];
        while (1) {
            size_t bytes_read = fread(buffer, 1, BLOCK_SIZE, new_file_ptr);
            if (bytes_read == 0) break;
            fwrite(buffer, 1, bytes_read, archive);
        }

        // Guardar la entrada de archivo para el nuevo archivo en las estructuras FAT y de entradas de archivo
        strcpy(file_entries[empty_entry_index].filename, new_file);
        file_entries[empty_entry_index].offset = archive_size;
        file_entries[empty_entry_index].size = new_file_size;

        // Actualizar el índice de la próxima entrada de archivo vacía
        empty_entry_index++;

        // Escribir las entradas de archivo actualizadas al archivo empacado
        fseek(archive, sizeof(FAT), SEEK_SET);
        fwrite(file_entries, sizeof(FileEntry), BLOCK_SIZE / sizeof(FileEntry), archive);

        // Cerrar el archivo nuevo
        fclose(new_file_ptr);

        if (verbose) {
            printf("Nuevo archivo agregado con éxito al archivo empacado: %s\n", new_file);
        }
    }

    // Cerrar el archivo empacado
    fclose(archive);

    if (verbose) {
        printf("Todos los nuevos archivos han sido agregados al archivo empacado: %s\n", archive_name);
    }
}

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

void extractArchive(char *archive_name, char **files_to_extract, int num_files, int verbose) {
    // Abrir el archivo empacado en modo lectura
    FILE *archive = fopen(archive_name, "rb");
    if (!archive) {
        printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Extrayendo archivos del archivo empacado: %s\n", archive_name);
    }

    // Leer la estructura FAT
    FAT fat;
    fread(&fat, sizeof(FAT), 1, archive);

    // Leer las entradas de archivo desde el archivo empacado
    FileEntry file_entries[BLOCK_SIZE / sizeof(FileEntry)];
    fread(file_entries, sizeof(FileEntry), BLOCK_SIZE / sizeof(FileEntry), archive);

    // Iterar sobre los archivos a extraer
    for (int i = 0; i < num_files; i++) {
        char *filename = files_to_extract[i];
        int file_found = 0;

        // Buscar el archivo en las entradas de archivo por su nombre
        for (int j = 0; j < BLOCK_SIZE / sizeof(FileEntry) && j < num_files; j++) {
            if (strcmp(file_entries[j].filename, filename) == 0) {
                // El archivo fue encontrado, extraer su contenido
                file_found = 1;
                uint64_t offset = file_entries[j].offset;
                uint64_t size = file_entries[j].size;

                // Mover el puntero de lectura/escritura al inicio del archivo dentro del archivo empacado
                fseek(archive, offset, SEEK_SET);

                // Crear un nuevo archivo para escribir el contenido extraído
                FILE *extracted_file = fopen(filename, "wb");
                if (!extracted_file) {
                    printf("Error: No se pudo crear el archivo extraído %s.\n", filename);
                    continue;
                }

                // Leer el contenido del archivo empacado y escribirlo al archivo extraído
                char buffer[BLOCK_SIZE];
                uint64_t bytes_left = size;
                while (bytes_left > 0) {
                    size_t bytes_to_read = (bytes_left < BLOCK_SIZE) ? bytes_left : BLOCK_SIZE;
                    size_t bytes_read = fread(buffer, 1, bytes_to_read, archive);
                    fwrite(buffer, 1, bytes_read, extracted_file);
                    bytes_left -= bytes_read;
                }

                fclose(extracted_file);

                if (verbose) {
                    printf("Archivo extraído: %s\n", filename);
                }

                break;
            }
        }

        if (!file_found) {
            printf("Error: El archivo %s no existe en el archivo empacado.\n", filename);
        }
    }

    // Cerrar el archivo empacado
    fclose(archive);

    if (verbose) {
        printf("Extracción completada.\n");
    }
}

// Función para desfragmentar el contenido del archivo empacado
void packArchive(char *archive_name, int verbose) {
    // Abrir el archivo empacado en modo lectura y escritura
    printf("HELLO\n");
    FILE *archive = fopen(archive_name, "r+b");
    if (!archive) {
        printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
        return;
    }

    if (verbose) {
        printf("Desfragmentando contenido del archivo empacado: %s\n", archive_name);
    }

    // Leer la estructura FAT
    FAT fat;
    fread(&fat, sizeof(FAT), 1, archive);

    // Leer las entradas de archivo desde el archivo empacado
    FileEntry file_entries[BLOCK_SIZE / sizeof(FileEntry)];
    fread(file_entries, sizeof(FileEntry), BLOCK_SIZE / sizeof(FileEntry), archive);

    // Reorganizar las entradas de archivo para eliminar los espacios no utilizados y compactar el contenido
    int num_files = BLOCK_SIZE / sizeof(FileEntry);
    int next_free_block = 0;
    for (int i = 0; i < num_files; i++) {
        if (file_entries[i].filename[0] != '\0') {
            if (next_free_block != i) {
                // Mover la entrada de archivo al siguiente bloque libre si es diferente de su posición actual
                file_entries[next_free_block] = file_entries[i];
                memset(&file_entries[i], 0, sizeof(FileEntry)); // Limpiar la entrada de archivo original
            }
            next_free_block++;
        }
    }

    // Escribir las entradas de archivo actualizadas al archivo empacado
    fseek(archive, sizeof(FAT), SEEK_SET);
    fwrite(file_entries, sizeof(FileEntry), num_files, archive);

    // Truncar el archivo empacado para eliminar los bloques no utilizados al final
    uint64_t new_size = sizeof(FAT) + next_free_block * sizeof(FileEntry);
    int result = ftruncate(fileno(archive), new_size);
    if (result != 0) {
        printf("Error: No se pudo truncar el archivo empacado %s.\n", archive_name);
        fclose(archive);
        return;
    }

    // Cerrar el archivo empacado
    fclose(archive);

    if (verbose) {
        printf("Desfragmentación completada. Tamaño del archivo empacado ajustado a %lu bytes.\n", new_size);
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
                    createArchive(archive_name, files_to_use, num_files, verbose);
                } else if (strcmp(option, "--update") == 0) {
                    updateArchive(archive_name, files_to_use, num_files, verbose);
                }else if (strcmp(option, "--list") == 0) {
                    listArchiveContents(archive_name, verbose);
                }else if (strcmp(option, "--append") == 0) {
                    appendArchive(archive_name, files_to_use, num_files, verbose);
                }else if (strcmp(option, "--extract") == 0) {
                    extractArchive(archive_name, files_to_use, num_files, verbose);
                }else if (strcmp(option, "--pack") == 0) {
                    packArchive(archive_name, verbose);
                }
                
            } else {
                // Forma abreviada de la opción
                for (int j = 1; option[j] != '\0'; j++) {
                    char opt = option[j];

                    switch (opt) {
                        case 'c':
                            createArchive(archive_name, files_to_use, num_files, verbose);
                            break;
                        case 'u':
                            updateArchive(archive_name, files_to_use, num_files, verbose);
                            break;
                        case 't':
                            listArchiveContents(archive_name, verbose);
                            break;
                        case 'r':
                            appendArchive(archive_name, files_to_use, num_files, verbose);
                            break;
                        case 'x':
                            extractArchive(archive_name, files_to_use, num_files, verbose);
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
//Este codigo contiene la funcion -x --extract y la funcion -p --pack
//gcc star5.c -o star
//./star -cvf prueba-paq.tar prueba2.docx prueba.txt 
//./star -pvf prueba-paq.tar