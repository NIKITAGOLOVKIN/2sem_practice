#include "bitIO.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Задаем предсказуемую последовательность битов
static int patternBit(int index)
{
    return (index * 17 + 3) & 1;
}

// Задаем предсказуемое значение по паттерну
static uint32_t patternValue(int start, int numberOfBits)
{
    uint32_t value = 0;

    for (int i = 0; i < numberOfBits; ++i)
        value = (value << 1) | (uint32_t)patternBit(start + i);

    return value;
}

// Пишет биты в файл и проверяет, что байты и повторное чтение совпадают с ними
static void writeAndReadBits(int numberOfBits)
{
    printf("--- writeAndReadBits started! ---\n");
    const char* path = "bitIOTest.bin";
    FILE* file;
    BitWriter writer;
    BitReader reader;
    int written = 0;
    uint8_t expected[32];
    uint8_t actual[32];
    int numberOfBytes;
    size_t readCount;

    file = fopen(path, "wb");
    assert(file != NULL);
    bitWriterInit(&writer, file);
    while (written < numberOfBits) {
        int chunk = numberOfBits - written;
        if (chunk > 32)
            chunk = 32;
        bitWriterWriteBits(&writer, patternValue(written, chunk), chunk);
        written += chunk;
    }
    bitWriterFlush(&writer);
    fclose(file);

    numberOfBytes = (numberOfBits + 7) / 8;
    for (int i = 0; i < numberOfBytes; ++i)
        expected[i] = 0;
    for (int i = 0; i < numberOfBits; ++i) {
        if (patternBit(i))
            expected[i / 8] |= (uint8_t)(1u << (7 - (i % 8)));
    }

    file = fopen(path, "rb");
    assert(file != NULL);
    readCount = fread(actual, 1, (size_t)numberOfBytes, file);
    assert(readCount == (size_t)numberOfBytes);
    assert(fgetc(file) == EOF);
    for (int i = 0; i < numberOfBytes; ++i)
        assert(actual[i] == expected[i]);
    fclose(file);

    file = fopen(path, "rb");
    assert(file != NULL);
    bitReaderInit(&reader, file);
    written = 0;
    while (written < numberOfBits) {
        int chunk = numberOfBits - written;
        uint32_t readValue = 0;
        if (chunk > 32)
            chunk = 32;
        assert(bitReaderReadBits(&reader, chunk, &readValue) == 0);
        assert(readValue == patternValue(written, chunk));
        written += chunk;
    }
    fclose(file);
    remove(path);
    printf("--- writeAndReadBits finished! ---\n");
}

// Проверяет, что 5 единиц после flush дают один байт с нулями в конце
static void testFlushPadsWithZeros(void)
{
    printf("--- testFlushPadsWithZeros started! ---\n");
    const char* path = "bitIOFlush.bin";
    FILE* file;
    BitWriter writer;
    int byte;

    file = fopen(path, "wb");
    assert(file != NULL);
    bitWriterInit(&writer, file);
    bitWriterWriteBits(&writer, 0x1F, 5);
    bitWriterFlush(&writer);
    fclose(file);

    file = fopen(path, "rb");
    assert(file != NULL);
    byte = fgetc(file);
    assert(byte == 0xF8);
    assert(fgetc(file) == EOF);
    fclose(file);
    remove(path);
    printf("--- testFlushPadsWithZeros finished! ---\n");
}

// Проверяет ошибку при чтении бита после конца файла
static void testReadPastEnd(void)
{
    printf("--- testReadPastEnd started! ---\n");
    const char* path = "bitIOEndOfFile.bin";
    FILE* file;
    BitWriter writer;
    BitReader reader;
    uint32_t readValue = 0;

    file = fopen(path, "wb");
    assert(file != NULL);
    bitWriterInit(&writer, file);
    bitWriterWriteBits(&writer, 0xA5, 8);
    bitWriterFlush(&writer);
    fclose(file);

    file = fopen(path, "rb");
    assert(file != NULL);
    bitReaderInit(&reader, file);
    assert(bitReaderReadBits(&reader, 8, &readValue) == 0);
    assert(readValue == 0xA5);
    assert(bitReaderReadBits(&reader, 1, &readValue) == -1);
    fclose(file);
    remove(path);
    printf("--- testReadPastEnd finished! ---\n");
}

int main(void)
{
    printf("--- testBitIo started! ---\n");
    const int lengths[] = { 1, 7, 15, 23, 100 };
    for (int i = 0; i < 5; ++i)
        writeAndReadBits(lengths[i]);
    testFlushPadsWithZeros();
    testReadPastEnd();
    printf("--- testBitIo finished! ---\n");
    return 0;
}
