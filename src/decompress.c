#include "decompress.h"
#include "bitIO.h"
#include "huffman.h"
#include <stdint.h>

enum {
    WRITE_CHUNK_SIZE = 64 * 1024
};

// Восстанавливает файл из потока битов и таблицы частот
int decompressStream(FILE* input, FILE* output, uint64_t originalSize, const uint32_t frequencies[256])
{
    uint64_t frequencySum = 0;
    HuffNode* root = NULL;
    BitReader reader;
    uint8_t chunk[WRITE_CHUNK_SIZE];
    size_t chunkLength = 0;
    uint64_t produced = 0;
    int symbol;

    if (originalSize > UINT32_MAX)
        return -1;

    for (symbol = 0; symbol < 256; ++symbol)
        frequencySum += frequencies[symbol];
    if (frequencySum != originalSize)
        return -1;
    if (originalSize == 0)
        return 0;

    if (huffBuildTree(frequencies, &root) != 0)
        return -1;
    if (huffCheckKraft(root) != 0) {
        huffFreeTree(root);
        return -1;
    }

    bitReaderInit(&reader, input);
    while (produced < originalSize) {
        const HuffNode* node = root;

        while (!huffIsLeaf(node)) {
            uint32_t bit = 0;

            if (bitReaderReadBits(&reader, 1, &bit) != 0) {
                huffFreeTree(root);
                return -1;
            }
            if (bit == 0)
                node = huffGetLeft(node);
            else
                node = huffGetRight(node);
            if (node == NULL) {
                huffFreeTree(root);
                return -1;
            }
        }

        chunk[chunkLength] = huffGetSymbol(node);
        chunkLength += 1;
        produced += 1;
        if (chunkLength == sizeof(chunk)) {
            if (fwrite(chunk, 1, chunkLength, output) != chunkLength) {
                huffFreeTree(root);
                return -1;
            }
            chunkLength = 0;
        }
    }

    if (chunkLength > 0 && fwrite(chunk, 1, chunkLength, output) != chunkLength) {
        huffFreeTree(root);
        return -1;
    }

    huffFreeTree(root);
    return 0;
}
