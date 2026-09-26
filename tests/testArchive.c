#include "archive.h"
#include <assert.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void makeDirectory(const char* path)
{
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0777);
#endif
}

static void removeDirectory(const char* path)
{
#ifdef _WIN32
    _rmdir(path);
#else
    rmdir(path);
#endif
}

static uint8_t patternByte(size_t index)
{
    return (uint8_t)(index * 17 + 3);
}

static void writeBuffer(const char* path, const uint8_t* data, size_t dataLength)
{
    FILE* file = fopen(path, "wb");
    assert(file != NULL);
    if (dataLength > 0)
        assert(fwrite(data, 1, dataLength, file) == dataLength);
    fclose(file);
}

static void writePattern(const char* path, size_t dataLength)
{
    FILE* file = fopen(path, "wb");
    size_t index = 0;

    assert(file != NULL);
    while (index < dataLength) {
        uint8_t chunk[64 * 1024];
        size_t chunkLength = dataLength - index;
        size_t i;

        if (chunkLength > sizeof(chunk))
            chunkLength = sizeof(chunk);
        for (i = 0; i < chunkLength; ++i)
            chunk[i] = patternByte(index + i);
        assert(fwrite(chunk, 1, chunkLength, file) == chunkLength);
        index += chunkLength;
    }
    fclose(file);
}

// Проверяем, что два файла совпадают
static void assertSameFile(const char* leftPath, const char* rightPath)
{
    FILE* left = fopen(leftPath, "rb");
    FILE* right = fopen(rightPath, "rb");
    uint8_t leftChunk[64 * 1024];
    uint8_t rightChunk[64 * 1024];

    assert(left != NULL && right != NULL);
    while (1) {
        size_t leftCount = fread(leftChunk, 1, sizeof(leftChunk), left);
        size_t rightCount = fread(rightChunk, 1, sizeof(rightChunk), right);
        size_t i;

        assert(leftCount == rightCount);
        for (i = 0; i < leftCount; ++i)
            assert(leftChunk[i] == rightChunk[i]);
        if (leftCount < sizeof(leftChunk)) {
            assert(!ferror(left) && !ferror(right));
            break;
        }
    }
    fclose(left);
    fclose(right);
}

// Читаем текстовый файл
static char* readTextFile(const char* path)
{
    FILE* file = fopen(path, "rb");
    long length;
    char* text;

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length >= 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    text = malloc((size_t)length + 1);
    assert(text != NULL);
    assert(fread(text, 1, (size_t)length, file) == (size_t)length);
    text[length] = '\0';
    fclose(file);
    return text;
}

// Подготавливаем тестовые файлы
static void prepareSampleFiles(void)
{
    const char text[] = "int main(void)\n{\n    return 0;\n}\n";
    const uint8_t zero = 0;
    uint8_t repeated[10000];
    uint8_t randomBytes[1000];
    uint32_t state = 1;
    int i;

    makeDirectory("archiveSample");
    makeDirectory("archiveSample/sub");
    writeBuffer("archiveSample/a.txt", (const uint8_t*)text, sizeof(text) - 1);
    writeBuffer("archiveSample/b.bin", &zero, 1);
    writeBuffer("archiveSample/empty.txt", NULL, 0);
    for (i = 0; i < 10000; ++i)
        repeated[i] = 'a';
    writeBuffer("archiveSample/repeated.txt", repeated, 10000);
    for (i = 0; i < 1000; ++i) {
        state = state * 1664525 + 1013904223;
        randomBytes[i] = (uint8_t)(state >> 24);
    }
    writeBuffer("archiveSample/random.bin", randomBytes, 1000);
    writeBuffer("archiveSample/sub/c.txt", (const uint8_t*)"inside the subdirectory\n", 21);
    writePattern("archiveSample/large.bin", (size_t)100 * 1024 * 1024);
}

// Тестируем архивацию и распаковку
static void testArchiveCompressAndDecompress(void)
{
    printf("--- testArchiveCompressAndDecompress started! ---\n");
    const char* inputFiles[] = {
        "archiveSample/a.txt",
        "archiveSample/empty.txt",
        "archiveSample/b.bin",
        "archiveSample/repeated.txt",
        "archiveSample/random.bin",
        "archiveSample/sub/c.txt",
        "archiveSample/large.bin"
    };
    FILE* listFile;
    char* listText;
    const char* extractedFiles[] = {
        "archiveOutput/archiveSample/a.txt",
        "archiveOutput/archiveSample/empty.txt",
        "archiveOutput/archiveSample/b.bin",
        "archiveOutput/archiveSample/repeated.txt",
        "archiveOutput/archiveSample/random.bin",
        "archiveOutput/archiveSample/sub/c.txt",
        "archiveOutput/archiveSample/large.bin"
    };
    int i;

    assert(archiveCreate("archiveSample.huf", inputFiles, 7) == 0);
    listFile = fopen("archiveSampleList.txt", "wb");
    assert(listFile != NULL);
    assert(archiveList("archiveSample.huf", listFile) == 0);
    fclose(listFile);

    listText = readTextFile("archiveSampleList.txt");
    assert(strstr(listText, "0  0  archiveSample/empty.txt\n") != NULL);
    assert(strstr(listText, "1  1  archiveSample/b.bin\n") != NULL);
    assert(strstr(listText, "10000  1250  archiveSample/repeated.txt\n") != NULL);
    assert(strstr(listText, "archiveSample/a.txt\n") != NULL);
    assert(strstr(listText, "archiveSample/random.bin\n") != NULL);
    assert(strstr(listText, "archiveSample/sub/c.txt\n") != NULL);
    assert(strstr(listText, "104857600  ") != NULL);
    free(listText);

    assert(archiveExtract("archiveSample.huf", "archiveOutput") == 0);
    for (i = 0; i < 7; ++i)
        assertSameFile(inputFiles[i], extractedFiles[i]);
    printf("--- testArchiveCompressAndDecompress finished! ---\n");
}

static int runArchiver(const char* arguments)
{
    char command[512];

#ifdef _WIN32
    snprintf(command, sizeof(command), "build\\huff.exe %s", arguments);
#else
    snprintf(command, sizeof(command), "build/huff %s", arguments);
#endif
    return system(command);
}

// Тестируем работу командной строки
static void testCommandLine(void)
{
    printf("--- testCommandLine started! ---\n");
    const char* inputFiles[] = {
        "archiveSample/a.txt",
        "archiveSample/b.bin",
        "archiveSample/sub/c.txt"
    };
    char* listText;
    int commandResult;

    commandResult = runArchiver("c archiveCommand.huf archiveSample/a.txt archiveSample/b.bin archiveSample/sub/c.txt");
    assert(commandResult == 0);
    commandResult = runArchiver("l archiveCommand.huf > archiveCommandList.txt");
    assert(commandResult == 0);
    listText = readTextFile("archiveCommandList.txt");
    assert(strstr(listText, "archiveSample/a.txt") != NULL);
    assert(strstr(listText, "archiveSample/b.bin") != NULL);
    assert(strstr(listText, "archiveSample/sub/c.txt") != NULL);
    free(listText);

    commandResult = runArchiver("x archiveCommand.huf archiveCommandOutput");
    assert(commandResult == 0);
    assertSameFile(inputFiles[0], "archiveCommandOutput/archiveSample/a.txt");
    assertSameFile(inputFiles[1], "archiveCommandOutput/archiveSample/b.bin");
    assertSameFile(inputFiles[2], "archiveCommandOutput/archiveSample/sub/c.txt");

    commandResult = runArchiver("c");
    assert(commandResult != 0);
    commandResult = runArchiver("no-such-command");
    assert(commandResult != 0);
    printf("--- testCommandLine finished! ---\n");
}

// Тестируем поврежденные архивы
static void testBrokenArchives(void)
{
    printf("--- testBrokenArchives started! ---\n");
    FILE* file;
    const unsigned char badVersion[] = { 'H', 'U', 'F', 'F', 2, 0, 0, 0, 0 };
    const unsigned char shortArchive[] = { 'H', 'U', 'F', 'F', 1 };

    file = fopen("archiveNotHuff.txt", "wb");
    assert(file != NULL);
    assert(fwrite("hello", 1, 5, file) == 5);
    fclose(file);
    assert(archiveExtract("archiveNotHuff.txt", "archiveBadOutput") == -1);

    file = fopen("archiveBadVersion.huf", "wb");
    assert(file != NULL);
    assert(fwrite(badVersion, 1, sizeof(badVersion), file) == sizeof(badVersion));
    fclose(file);
    assert(archiveExtract("archiveBadVersion.huf", "archiveBadOutput") == -1);

    file = fopen("archiveShort.huf", "wb");
    assert(file != NULL);
    assert(fwrite(shortArchive, 1, sizeof(shortArchive), file) == sizeof(shortArchive));
    fclose(file);
    assert(archiveExtract("archiveShort.huf", "archiveBadOutput") == -1);
    assert(archiveList("archiveShort.huf", stdout) == -1);
    printf("--- testBrokenArchives finished! ---\n");
}

// Удаляет тестовые файлы
static void removeSampleFiles(void)
{
    remove("archiveSample/a.txt");
    remove("archiveSample/b.bin");
    remove("archiveSample/empty.txt");
    remove("archiveSample/repeated.txt");
    remove("archiveSample/random.bin");
    remove("archiveSample/sub/c.txt");
    remove("archiveSample/large.bin");
    removeDirectory("archiveSample/sub");
    removeDirectory("archiveSample");

    remove("archiveOutput/archiveSample/a.txt");
    remove("archiveOutput/archiveSample/b.bin");
    remove("archiveOutput/archiveSample/empty.txt");
    remove("archiveOutput/archiveSample/repeated.txt");
    remove("archiveOutput/archiveSample/random.bin");
    remove("archiveOutput/archiveSample/sub/c.txt");
    remove("archiveOutput/archiveSample/large.bin");
    removeDirectory("archiveOutput/archiveSample/sub");
    removeDirectory("archiveOutput/archiveSample");
    removeDirectory("archiveOutput");

    remove("archiveCommandOutput/archiveSample/a.txt");
    remove("archiveCommandOutput/archiveSample/b.bin");
    remove("archiveCommandOutput/archiveSample/sub/c.txt");
    removeDirectory("archiveCommandOutput/archiveSample/sub");
    removeDirectory("archiveCommandOutput/archiveSample");
    removeDirectory("archiveCommandOutput");

    remove("archiveSample.huf");
    remove("archiveSampleList.txt");
    remove("archiveCommand.huf");
    remove("archiveCommandList.txt");
    remove("archiveNotHuff.txt");
    remove("archiveBadVersion.huf");
    remove("archiveShort.huf");
}

// Запуск тестов
int main(void)
{
    printf("--- testArchive started! ---\n");
    removeSampleFiles();
    prepareSampleFiles();
    testArchiveCompressAndDecompress();
    testCommandLine();
    testBrokenArchives();
    removeSampleFiles();
    printf("--- testArchive finished! ---\n");
    return 0;
}
