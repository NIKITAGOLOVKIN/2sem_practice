#include "archive.h"
#include <stdio.h>
#include <string.h>

static void printUsage(FILE* output)
{
    fprintf(output,
        "Usage: huff <command> [args...]\n"
        "  huff c <archive.huf> <file1> file2 ... - создать архив из списка файлов\n"
        "  huff x <archive.huf> [outDir] - извлечь архив в каталог (по умолчанию текущий каталог)\n"
        "  huff l <archive.huf> - список: исходный размер, сжатый размер, путь\n"
        "  huff --help - вывести справку\n");
}

int main(int argc, char** argv)
{
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Недостаточно аргументов\n");
            printUsage(stderr);
            return 1;
        }
        printUsage(stdout);
        return 0;
    }

    if (strcmp(argv[1], "c") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Недостаточно аргументов\n");
            printUsage(stderr);
            return 1;
        }
        if (archiveCreate(argv[2], (const char**)(argv + 3), (size_t)(argc - 3)) != 0)
            return 1;
        return 0;
    }

    if (strcmp(argv[1], "x") == 0) {
        const char* outputDirectory = ".";

        if (argc < 3 || argc > 4) {
            fprintf(stderr, "Недостаточно аргументов\n");
            printUsage(stderr);
            return 1;
        }
        if (argc == 4)
            outputDirectory = argv[3];
        if (archiveExtract(argv[2], outputDirectory) != 0)
            return 1;
        return 0;
    }

    if (strcmp(argv[1], "l") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Недостаточно аргументов\n");
            printUsage(stderr);
            return 1;
        }
        if (archiveList(argv[2], stdout) != 0)
            return 1;
        return 0;
    }

    fprintf(stderr, "Неизвестная команда: %s\n", argv[1]);
    printUsage(stderr);
    return 1;
}