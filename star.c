#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define BLOCK_SIZE 256 * 1024

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
void createArchive(char *archive_name, char **files_to_pack, int num_files) {
    // Implementar la lógica para crear un nuevo archivo empacado
    // y almacenar los archivos especificados en él
}

// Función para extraer archivos de un archivo empacado
void extractArchive(char *archive_name) {
    // Implementar la lógica para extraer archivos del archivo empacado
}

// Función para listar contenidos de un archivo empacado
void listArchive(char *archive_name) {
    // Implementar la lógica para listar los contenidos del archivo empacado
}
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <opciones> <archivo_empacado> [archivos]\n", argv[0]);
        return 1;
    }

    char *archive_name = argv[argc - 1];
    char **files_to_pack = &argv[2];
    int num_files = argc - 3;

    for (int i = 1; i < argc - 1; i++) {
        char *option = argv[i];

        if (strcmp(option, "-c") == 0 || strcmp(option, "--create") == 0) {
            createArchive(archive_name, files_to_pack, num_files);
        } else if (strcmp(option, "-x") == 0 || strcmp(option, "--extract") == 0) {
            extractArchive(archive_name);
        } else if (strcmp(option, "-t") == 0 || strcmp(option, "--list") == 0) {
            listArchive(archive_name);
        } else if (strcmp(option, "--delete") == 0) {
            if (i + 1 >= argc) {
                printf("Uso: %s --delete <archivo_empacado> [archivos]\n", argv[0]);
                return 1;
            }
            deleteFromArchive(archive_name, &argv[i + 1], argc - i - 2);
            break; // No se procesan más opciones después de --delete
        } else if (strcmp(option, "-u") == 0 || strcmp(option, "--update") == 0) {
            updateArchive(archive_name, files_to_pack, num_files);
        } else if (strcmp(option, "-r") == 0 || strcmp(option, "--append") == 0) {
            appendToArchive(archive_name, files_to_pack, num_files);
        } else if (strcmp(option, "-p") == 0 || strcmp(option, "--pack") == 0) {
            packArchive(archive_name);
        } else {
            printf("Opción no válida: %s\n", option);
            return 1;
        }
    }

    return 0;
}

