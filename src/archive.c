#include "archive.h"
#include "compress.h"
#include "decompress.h"
#include <direct.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ARCHIVE_VERSION = 1,
    MAGIC_LENGTH = 4,
    FREQUENCY_COUNT = 256,
    COPY_CHUNK_SIZE = 64 * 1024
};

static const char MAGIC[MAGIC_LENGTH] = { 'H', 'U', 'F', 'F' };

// Пишет целое маленьким концом вперёд
static int writeLittleEndian(FILE* file, uint64_t value, int width)
{
    int i;

    for (i = 0; i < width; ++i) {
        if (fputc((unsigned char)(value & 0xffu), file) == EOF)
            return -1;
        value >>= 8;
    }
    return 0;
}

// Читает целое маленьким концом вперёд
static int readLittleEndian(FILE* file, uint64_t* value, int width)
{
    unsigned char bytes[8];
    uint64_t result = 0;
    int i;

    if (width < 1 || width > 8)
        return -1;
    if (fread(bytes, 1, (size_t)width, file) != (size_t)width)
        return -1;
    for (i = 0; i < width; ++i)
        result |= (uint64_t)bytes[i] << (8 * i);
    *value = result;
    return 0;
}

// Копирует данные из временного файла в архив
static int copyStream(FILE* input, FILE* output, uint64_t byteCount)
{
    unsigned char chunk[COPY_CHUNK_SIZE];

    while (byteCount > 0) {
        size_t chunkLength = byteCount > sizeof(chunk) ? sizeof(chunk) : (size_t)byteCount;
        if (fread(chunk, 1, chunkLength, input) != chunkLength)
            return -1;
        if (fwrite(chunk, 1, chunkLength, output) != chunkLength)
            return -1;
        byteCount -= chunkLength;
    }
    return 0;
}

// Пропуск сжатых данных
static int skipBytes(FILE* file, uint64_t byteCount)
{
    unsigned char chunk[COPY_CHUNK_SIZE];

    while (byteCount > 0) {
        size_t chunkLength = byteCount > sizeof(chunk) ? sizeof(chunk) : (size_t)byteCount;
        if (fread(chunk, 1, chunkLength, file) != chunkLength)
            return -1;
        byteCount -= chunkLength;
    }
    return 0;
}

// 1, если путь относительный и в нём нет выхода наверх через ..
static int pathIsSafe(const char* path)
{
    const char* cursor = path;

    if (path[0] == '\0' || path[0] == '/' || path[0] == '\\' || strchr(path, ':') != NULL)
        return 0;

    while (*cursor != '\0') {
        const char* start = cursor;
        size_t length = 0;

        while (*cursor != '\0' && *cursor != '/' && *cursor != '\\') {
            ++cursor;
            ++length;
        }
        if (length == 0 || (length == 1 && start[0] == '.') || (length == 2 && start[0] == '.' && start[1] == '.'))
            return 0;
        if (*cursor != '\0')
            ++cursor;
    }
    return 1;
}

static int createDirectory(const char* path)
{
    if (path[0] == '\0' || strcmp(path, ".") == 0)
        return 0;

    if (_mkdir(path) == 0 || errno == EEXIST)
        return 0;
    return -1;
}

// Создаёт все каталоги по пути, кроме последнего компонента — это имя файла
static int ensureParentDirectories(char* path)
{
    char* cursor;

    for (cursor = path; *cursor != '\0'; ++cursor) {
        char separator;

        if (*cursor != '/' && *cursor != '\\')
            continue;
        if (cursor == path)
            return -1;

        separator = *cursor;
        *cursor = '\0';
        if (createDirectory(path) != 0) {
            *cursor = separator;
            return -1;
        }
        *cursor = separator;
    }
    return 0;
}

// Объединение путей
static int joinPaths(char* destination, size_t destinationSize, const char* directory, const char* relativePath)
{
    size_t directoryLength = strlen(directory);
    size_t relativeLength = strlen(relativePath);
    int needsSlash = 1;

    if (directoryLength == 0 || directory[directoryLength - 1] == '/' || directory[directoryLength - 1] == '\\')
        needsSlash = 0;
    if (directoryLength + (size_t)needsSlash + relativeLength + 1 > destinationSize)
        return -1;

    memcpy(destination, directory, directoryLength);
    if (needsSlash)
        destination[directoryLength] = '/';
    memcpy(destination + directoryLength + (size_t)needsSlash, relativePath, relativeLength + 1);
    return 0;
}

static void reportTruncatedArchive(void)
{
    fprintf(stderr, "Архив обрезан или повреждён\n");
}

// Читает заголовок архива
static int readArchiveHeader(FILE* archive, uint32_t* numberOfFiles)
{
    char magic[MAGIC_LENGTH];
    uint64_t version = 0;
    uint64_t fileCount = 0;

    if (fread(magic, 1, MAGIC_LENGTH, archive) != MAGIC_LENGTH) {
        reportTruncatedArchive();
        return -1;
    }
    if (memcmp(magic, MAGIC, MAGIC_LENGTH) != 0) {
        fprintf(stderr, "Не HUFF-архив\n");
        return -1;
    }
    if (readLittleEndian(archive, &version, 1) != 0) {
        reportTruncatedArchive();
        return -1;
    }
    if (version != ARCHIVE_VERSION) {
        fprintf(stderr, "Неподдерживаемая версия архива\n");
        return -1;
    }
    if (readLittleEndian(archive, &fileCount, 4) != 0) {
        reportTruncatedArchive();
        return -1;
    }
    *numberOfFiles = (uint32_t)fileCount;
    return 0;
}

// Читает данные файла из архива
static int readFileEntry(FILE* archive, char** path, uint64_t* originalSize, uint64_t* compressedSize, uint32_t frequencies[FREQUENCY_COUNT])
{
    uint64_t pathLength = 0;
    uint64_t size = 0;
    int symbol;
    char* pathBytes;

    if (readLittleEndian(archive, &pathLength, 2) != 0) {
        reportTruncatedArchive();
        return -1;
    }
    pathBytes = malloc((size_t)pathLength + 1);
    if (pathBytes == NULL) {
        fprintf(stderr, "Не хватило памяти\n");
        return -1;
    }
    if (pathLength > 0 && fread(pathBytes, 1, (size_t)pathLength, archive) != (size_t)pathLength) {
        free(pathBytes);
        reportTruncatedArchive();
        return -1;
    }
    pathBytes[pathLength] = '\0';
    if (!pathIsSafe(pathBytes)) {
        free(pathBytes);
        fprintf(stderr, "Недопустимый путь в архиве\n");
        return -1;
    }

    if (readLittleEndian(archive, originalSize, 8) != 0 || readLittleEndian(archive, compressedSize, 8) != 0) {
        free(pathBytes);
        reportTruncatedArchive();
        return -1;
    }
    for (symbol = 0; symbol < FREQUENCY_COUNT; ++symbol) {
        if (readLittleEndian(archive, &size, 4) != 0) {
            free(pathBytes);
            reportTruncatedArchive();
            return -1;
        }
        frequencies[symbol] = (uint32_t)size;
    }

    *path = pathBytes;
    return 0;
}

// Создаёт архив из списка файлов
int archiveCreate(const char* archivePath, const char** inputFiles, size_t numberOfFiles)
{
    FILE* archive;
    size_t index;

    if (numberOfFiles > UINT32_MAX) {
        fprintf(stderr, "Слишком много файлов\n");
        return -1;
    }

    archive = fopen(archivePath, "wb");
    if (archive == NULL) {
        fprintf(stderr, "Не удалось открыть файл: %s\n", archivePath);
        return -1;
    }
    if (fwrite(MAGIC, 1, MAGIC_LENGTH, archive) != MAGIC_LENGTH
        || writeLittleEndian(archive, ARCHIVE_VERSION, 1) != 0
        || writeLittleEndian(archive, numberOfFiles, 4) != 0) {
        fclose(archive);
        remove(archivePath);
        fprintf(stderr, "Не удалось записать архив: %s\n", archivePath);
        return -1;
    }

    for (index = 0; index < numberOfFiles; ++index) {
        const char* inputPath = inputFiles[index];
        size_t pathLength = strlen(inputPath);
        FILE* input;
        FILE* compressed;
        uint64_t originalSize = 0;
        uint64_t compressedSize = 0;
        uint32_t frequencies[FREQUENCY_COUNT];
        int symbol;

        if (pathLength == 0 || pathLength > UINT16_MAX || !pathIsSafe(inputPath)) {
            fclose(archive);
            remove(archivePath);
            fprintf(stderr, "Недопустимый путь: %s\n", inputPath);
            return -1;
        }

        input = fopen(inputPath, "rb");
        compressed = tmpfile();
        if (input == NULL || compressed == NULL) {
            if (input != NULL)
                fclose(input);
            if (compressed != NULL)
                fclose(compressed);
            fclose(archive);
            remove(archivePath);
            fprintf(stderr, "Не удалось открыть файл: %s\n", inputPath);
            return -1;
        }
        if (compressStream(input, compressed, &originalSize, &compressedSize, frequencies) != 0
            || fseek(compressed, 0, SEEK_SET) != 0) {
            fclose(input);
            fclose(compressed);
            fclose(archive);
            remove(archivePath);
            fprintf(stderr, "Не удалось сжать файл: %s\n", inputPath);
            return -1;
        }
        fclose(input);

        if (writeLittleEndian(archive, pathLength, 2) != 0
            || fwrite(inputPath, 1, pathLength, archive) != pathLength
            || writeLittleEndian(archive, originalSize, 8) != 0
            || writeLittleEndian(archive, compressedSize, 8) != 0) {
            fclose(compressed);
            fclose(archive);
            remove(archivePath);
            fprintf(stderr, "Не удалось записать архив: %s\n", archivePath);
            return -1;
        }
        for (symbol = 0; symbol < FREQUENCY_COUNT; ++symbol) {
            if (writeLittleEndian(archive, frequencies[symbol], 4) != 0) {
                fclose(compressed);
                fclose(archive);
                remove(archivePath);
                fprintf(stderr, "Не удалось записать архив: %s\n", archivePath);
                return -1;
            }
        }
        if (copyStream(compressed, archive, compressedSize) != 0) {
            fclose(compressed);
            fclose(archive);
            remove(archivePath);
            fprintf(stderr, "Не удалось записать архив: %s\n", archivePath);
            return -1;
        }
        fclose(compressed);
    }

    if (ferror(archive)) {
        fclose(archive);
        remove(archivePath);
        fprintf(stderr, "Не удалось записать архив: %s\n", archivePath);
        return -1;
    }
    fclose(archive);
    return 0;
}

// Извлекает архив в каталог
int archiveExtract(const char* archivePath, const char* outputDirectory)
{
    FILE* archive;
    uint32_t numberOfFiles = 0;
    uint32_t index;
    const char* directory = outputDirectory == NULL ? "." : outputDirectory;

    archive = fopen(archivePath, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Не удалось открыть файл: %s\n", archivePath);
        return -1;
    }
    if (readArchiveHeader(archive, &numberOfFiles) != 0) {
        fclose(archive);
        return -1;
    }

    for (index = 0; index < numberOfFiles; ++index) {
        char* relativePath = NULL;
        char* outputPath;
        size_t outputPathSize;
        uint64_t originalSize = 0;
        uint64_t compressedSize = 0;
        uint32_t frequencies[FREQUENCY_COUNT];
        FILE* output;

        if (readFileEntry(archive, &relativePath, &originalSize, &compressedSize, frequencies) != 0) {
            fclose(archive);
            return -1;
        }

        outputPathSize = strlen(directory) + strlen(relativePath) + 2;
        outputPath = malloc(outputPathSize);
        if (outputPath == NULL || joinPaths(outputPath, outputPathSize, directory, relativePath) != 0 || ensureParentDirectories(outputPath) != 0) {
            free(relativePath);
            free(outputPath);
            fclose(archive);
            fprintf(stderr, "Не удалось создать каталог для распаковки\n");
            return -1;
        }

        output = fopen(outputPath, "wb");
        if (output == NULL) {
            fprintf(stderr, "Не удалось открыть файл: %s\n", outputPath);
            free(relativePath);
            free(outputPath);
            fclose(archive);
            return -1;
        }
        {
            long dataStart = ftell(archive);
            long dataEnd;
            uint64_t consumed;

            if (dataStart < 0 || decompressStream(archive, output, originalSize, frequencies) != 0) {
                fprintf(stderr, "Не удалось распаковать файл: %s\n", relativePath);
                fclose(output);
                free(relativePath);
                free(outputPath);
                fclose(archive);
                return -1;
            }
            dataEnd = ftell(archive);
            if (dataEnd < dataStart) {
                fprintf(stderr, "Не удалось распаковать файл: %s\n", relativePath);
                fclose(output);
                free(relativePath);
                free(outputPath);
                fclose(archive);
                return -1;
            }
            consumed = (uint64_t)(dataEnd - dataStart);
            if (consumed > compressedSize || skipBytes(archive, compressedSize - consumed) != 0) {
                fprintf(stderr, "Не удалось распаковать файл: %s\n", relativePath);
                fclose(output);
                free(relativePath);
                free(outputPath);
                fclose(archive);
                return -1;
            }
        }
        fclose(output);
        free(relativePath);
        free(outputPath);
    }

    fclose(archive);
    return 0;
}

// Список: исходный размер, сжатый размер, путь
int archiveList(const char* archivePath, FILE* output)
{
    FILE* archive;
    uint32_t numberOfFiles = 0;
    uint32_t index;

    archive = fopen(archivePath, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Не удалось открыть файл: %s\n", archivePath);
        return -1;
    }
    if (readArchiveHeader(archive, &numberOfFiles) != 0) {
        fclose(archive);
        return -1;
    }

    for (index = 0; index < numberOfFiles; ++index) {
        char* relativePath = NULL;
        uint64_t originalSize = 0;
        uint64_t compressedSize = 0;
        uint32_t frequencies[FREQUENCY_COUNT];

        if (readFileEntry(archive, &relativePath, &originalSize, &compressedSize, frequencies) != 0) {
            fclose(archive);
            return -1;
        }
        fprintf(output, "%llu  %llu  %s\n", (unsigned long long)originalSize, (unsigned long long)compressedSize, relativePath);
        free(relativePath);
        if (skipBytes(archive, compressedSize) != 0) {
            reportTruncatedArchive();
            fclose(archive);
            return -1;
        }
    }

    fclose(archive);
    return 0;
}
