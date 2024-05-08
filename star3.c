#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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

        // Buscar el archivo en las entradas de archivo por su ruta completa
        for (int j = 0; j < num_files; j++) {
            if (strcmp(file_entries[j].filename, filename) == 0) {
                // El archivo fue encontrado, actualizar su contenido
                file_found = 1;
                uint64_t offset = file_entries[j].offset;
                uint64_t size = file_entries[j].size;

                // Mover el puntero de lectura/escritura al inicio del archivo dentro del archivo empacado
                fseek(archive, offset, SEEK_SET);

                // Leer el contenido del archivo original
                char *buffer = (char *)malloc(size);
                fread(buffer, size, 1, archive);

                // Cerrar el archivo para abrirlo en modo escritura
                fclose(archive);
                archive = fopen(archive_name, "r+b");
                if (!archive) {
                    printf("Error: No se pudo abrir el archivo empacado %s.\n", archive_name);
                    free(buffer);
                    return;
                }
                fseek(archive, offset, SEEK_SET);

                // Abrir el archivo a actualizar en modo lectura
                FILE *updated_file = fopen(filename, "rb");
                if (!updated_file) {
                    printf("Error: No se pudo abrir el archivo %s.\n", filename);
                    free(buffer);
                    continue;
                }

                // Escribir el contenido actualizado al archivo empacado
                while (!feof(updated_file)) {
                    size_t bytes_read = fread(buffer, 1, BLOCK_SIZE, updated_file);
                    if (bytes_read == 0) break;
                    fwrite(buffer, 1, bytes_read, archive);
                }

                // Cerrar el archivo actualizado
                fclose(updated_file);
                free(buffer);

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
                            printf("%s \n", archive_name);
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
                    }
                }
            }
        }
    }

    return 0;
}

//gcc star3.c -o star
//./star -cvf prueba-paq.tar prueba.txt
//./star --create --verbose --file prueba-paq.tar prueba.txt
//./star -cvf prueba-paq.tar prueba.txt prueba2.docx