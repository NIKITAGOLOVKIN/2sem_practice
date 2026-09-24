#include "compress.h"
#include "bitIO.h"
#include "huffman.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    READ_CHUNK_SIZE = 64 * 1024
};

// Читает весь файл в память
static int readWholeFile(FILE* input, uint8_t** outputData, size_t* outputLength)
{
    uint8_t* data = NULL;
    size_t length = 0;
    size_t capacity = 0;
    uint8_t chunk[READ_CHUNK_SIZE];

    while (1) {
        size_t readCount = fread(chunk, 1, sizeof(chunk), input);

        if (readCount > 0) {
            size_t newCapacity;
            uint8_t* grown;

            if (length > UINT32_MAX - readCount) {
                free(data);
                return -1;
            }
            newCapacity = capacity == 0 ? sizeof(chunk) : capacity;
            while (newCapacity < length + readCount) {
                if (newCapacity > SIZE_MAX / 2) {
                    free(data);
                    return -1;
                }
                newCapacity *= 2;
            }
            if (newCapacity != capacity) {
                grown = realloc(data, newCapacity);
                if (grown == NULL) {
                    free(data);
                    return -1;
                }
                data = grown;
                capacity = newCapacity;
            }
            memcpy(data + length, chunk, readCount);
            length += readCount;
        }

        if (readCount < sizeof(chunk)) {
            if (ferror(input)) {
                free(data);
                return -1;
            }
            break;
        }
    }

    *outputData = data;
    *outputLength = length;
    return 0;
}

// Пишет код символа со старшего бита, кусками не длиннее 32 бит
static void writeCode(BitWriter* writer, const Code* code)
{
    int written = 0;

    while (written < code->length) {
        int chunk = code->length - written;
        uint32_t value = 0;
        int i;

        if (chunk > 32)
            chunk = 32;
        for (i = 0; i < chunk; ++i) {
            int bitIndex = written + i;
            uint32_t bit = (uint32_t)((code->code[bitIndex / 8] >> (7 - (bitIndex % 8))) & 1);
            value = (value << 1) | bit;
        }
        bitWriterWriteBits(writer, value, chunk);
        written += chunk;
    }
}

// Сжимает один файл в поток битов и таблицу частот
int compressStream(FILE* input, FILE* output, uint64_t* originalSize, uint64_t* compressedSize, uint32_t frequencies[256])
{
    uint8_t* data = NULL;
    size_t dataLength = 0;
    HuffNode* root = NULL;
    CodeTable table;
    BitWriter writer;
    uint64_t totalBitCount = 0;
    size_t index;

    if (readWholeFile(input, &data, &dataLength) != 0)
        return -1;
    if (huffCountFrequencies(data, dataLength, frequencies) != 0) {
        free(data);
        return -1;
    }
    if (huffBuildTree(frequencies, &root) != 0) {
        free(data);
        return -1;
    }

    huffBuildCodeTable(root, table);
    bitWriterInit(&writer, output);
    for (index = 0; index < dataLength; ++index) {
        const Code* code = &table[data[index]];
        if (code->length <= 0) {
            huffFreeTree(root);
            free(data);
            return -1;
        }
        writeCode(&writer, code);
        totalBitCount += (uint64_t)code->length;
    }
    bitWriterFlush(&writer);
    if (ferror(output)) {
        huffFreeTree(root);
        free(data);
        return -1;
    }

    *originalSize = dataLength;
    *compressedSize = (totalBitCount + 7) / 8;
    huffFreeTree(root);
    free(data);
    return 0;
}
