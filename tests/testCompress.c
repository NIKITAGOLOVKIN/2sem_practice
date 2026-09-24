#include "compress.h"
#include "decompress.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t patternByte(size_t index)
{
    return (uint8_t)(index * 17 + 3);
}

static void removeTempFiles(void)
{
    remove("compressInput.bin");
    remove("compressData.bin");
    remove("compressOutput.bin");
}

static void writeInput(const uint8_t* data, size_t dataLength, int usePattern)
{
    FILE* file = fopen("compressInput.bin", "wb");
    size_t index = 0;

    assert(file != NULL);
    while (index < dataLength) {
        uint8_t chunk[64 * 1024];
        size_t chunkLength = dataLength - index;
        size_t i;

        if (chunkLength > sizeof(chunk))
            chunkLength = sizeof(chunk);
        for (i = 0; i < chunkLength; ++i) {
            if (usePattern)
                chunk[i] = patternByte(index + i);
            else
                chunk[i] = data[index + i];
        }
        assert(fwrite(chunk, 1, chunkLength, file) == chunkLength);
        index += chunkLength;
    }
    fclose(file);
}

static void checkOutput(const uint8_t* data, size_t dataLength, int usePattern)
{
    FILE* file = fopen("compressOutput.bin", "rb");
    size_t index = 0;
    uint8_t chunk[64 * 1024];

    assert(file != NULL);
    while (1) {
        size_t readCount = fread(chunk, 1, sizeof(chunk), file);
        size_t i;

        for (i = 0; i < readCount; ++i) {
            uint8_t expected = usePattern ? patternByte(index) : data[index];
            assert(index < dataLength);
            assert(chunk[i] == expected);
            index += 1;
        }
        if (readCount < sizeof(chunk)) {
            assert(!ferror(file));
            break;
        }
    }
    assert(index == dataLength);
    fclose(file);
}

// Сжимает файл и тут же распаковывает, байты должны совпасть
static void compressAndDecompress(const uint8_t* data, size_t dataLength, int usePattern)
{
    FILE* input;
    FILE* compressed;
    FILE* output;
    uint64_t originalSize = 1;
    uint64_t compressedSize = 1;
    uint32_t frequencies[256];
    uint64_t frequencySum = 0;
    long dataEnd;
    int symbol;

    removeTempFiles();
    writeInput(data, dataLength, usePattern);

    input = fopen("compressInput.bin", "rb");
    compressed = fopen("compressData.bin", "wb");
    assert(input != NULL && compressed != NULL);
    assert(compressStream(input, compressed, &originalSize, &compressedSize, frequencies) == 0);
    fclose(input);
    fclose(compressed);

    assert(originalSize == dataLength);
    frequencySum = 0;
    for (symbol = 0; symbol < 256; ++symbol)
        frequencySum += frequencies[symbol];
    assert(frequencySum == originalSize);

    compressed = fopen("compressData.bin", "rb");
    assert(compressed != NULL);
    assert(fseek(compressed, 0, SEEK_END) == 0);
    dataEnd = ftell(compressed);
    assert(dataEnd >= 0);
    assert((uint64_t)dataEnd == compressedSize);
    assert(fseek(compressed, 0, SEEK_SET) == 0);

    output = fopen("compressOutput.bin", "wb");
    assert(output != NULL);
    assert(decompressStream(compressed, output, originalSize, frequencies) == 0);
    fclose(compressed);
    fclose(output);

    checkOutput(data, dataLength, usePattern);
    removeTempFiles();
}

// Короткий текст вроде исходника на C
static void testSourceText(void)
{
    printf("--- testSourceText started! ---\n");
    const char text[] = "int main(void)\n{\n    return 0;\n}\n";
    compressAndDecompress((const uint8_t*)text, sizeof(text) - 1, 0);
    printf("--- testSourceText finished! ---\n");
}

// Пустой файл
static void testEmptyFile(void)
{
    printf("--- testEmptyFile started! ---\n");
    uint32_t frequencies[256];
    int symbol;

    compressAndDecompress(NULL, 0, 0);
    writeInput(NULL, 0, 0);
    {
        FILE* input = fopen("compressInput.bin", "rb");
        FILE* compressed = fopen("compressData.bin", "wb");
        uint64_t originalSize = 1;
        uint64_t compressedSize = 1;

        assert(input != NULL && compressed != NULL);
        assert(compressStream(input, compressed, &originalSize, &compressedSize, frequencies) == 0);
        fclose(input);
        fclose(compressed);
        assert(originalSize == 0);
        assert(compressedSize == 0);
        for (symbol = 0; symbol < 256; ++symbol)
            assert(frequencies[symbol] == 0);
    }
    removeTempFiles();
    printf("--- testEmptyFile finished! ---\n");
}

// Разное количество байт
static void testSizes(void)
{
    printf("--- testSizes started! ---\n");
    uint8_t oneByte = 0;
    uint8_t alphabet[256];
    int symbol;

    compressAndDecompress(&oneByte, 1, 0);
    for (symbol = 0; symbol < 256; ++symbol)
        alphabet[symbol] = (uint8_t)symbol;
    compressAndDecompress(alphabet, 256, 0);
    compressAndDecompress(NULL, 1024 * 1024, 1);
    compressAndDecompress(NULL, (size_t)100 * 1024 * 1024, 1);
    printf("--- testSizes finished! ---\n");
}

// Поврежденные частоты
static void testDamagedFrequencies(void)
{
    printf("--- testDamagedFrequencies started! ---\n");
    uint32_t frequencies[256];
    FILE* input;
    FILE* output;
    int symbol;

    for (symbol = 0; symbol < 256; ++symbol)
        frequencies[symbol] = 0;

    removeTempFiles();
    writeInput(NULL, 0, 0);
    input = fopen("compressInput.bin", "rb");
    output = fopen("compressOutput.bin", "wb");
    assert(input != NULL && output != NULL);
    assert(decompressStream(input, output, 4, frequencies) == -1);
    fclose(input);
    fclose(output);

    frequencies[0] = 1;
    input = fopen("compressInput.bin", "rb");
    output = fopen("compressOutput.bin", "wb");
    assert(input != NULL && output != NULL);
    assert(decompressStream(input, output, 1, frequencies) == -1);
    fclose(input);
    fclose(output);
    removeTempFiles();
    printf("--- testDamagedFrequencies finished! ---\n");
}

// Запускает все тесты
int main(void)
{
    printf("--- testCompress started! ---\n");
    testSourceText();
    testEmptyFile();
    testSizes();
    testDamagedFrequencies();
    printf("--- testCompress finished! ---\n");
    return 0;
}
