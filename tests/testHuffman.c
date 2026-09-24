#include "bitIO.h"
#include "huffman.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int codesSharePrefix(const Code* leftCode, const Code* rightCode)
{
    const Code* shorter = leftCode;
    const Code* longer = rightCode;

    if (leftCode->length == 0 || rightCode->length == 0)
        return 0;
    if (leftCode->length > rightCode->length) {
        shorter = rightCode;
        longer = leftCode;
    }

    for (int i = 0; i < shorter->length; ++i) {
        int shorterBit = (shorter->code[i / 8] >> (7 - (i % 8))) & 1;
        int longerBit = (longer->code[i / 8] >> (7 - (i % 8))) & 1;
        if (shorterBit != longerBit)
            return 0;
    }
    return 1;
}

// Проверить, что коды не являются префиксами друг друга
static void assertPrefixFree(const CodeTable table, const uint32_t frequencies[256])
{
    for (int left = 0; left < 256; ++left) {
        if (frequencies[left] == 0)
            continue;
        assert(table[left].length > 0);
        for (int right = left + 1; right < 256; ++right) {
            if (frequencies[right] == 0)
                continue;
            assert(!codesSharePrefix(&table[left], &table[right]));
        }
    }
}

static int codeCost(const CodeTable table, const uint32_t frequencies[256])
{
    int cost = 0;
    int symbol;

    for (symbol = 0; symbol < 256; ++symbol)
        cost += (int)frequencies[symbol] * table[symbol].length;
    return cost;
}

// Пустой буфер не строит дерево
static void testEmptyInput(void)
{
    printf("--- testEmptyInput started! ---\n");
    uint32_t frequencies[256];
    HuffNode* root = (HuffNode*)(uintptr_t)1;

    assert(huffCountFrequencies(NULL, 0, frequencies) == 0);
    assert(huffBuildTree(frequencies, &root) == 0);
    assert(root == NULL);
    if (sizeof(size_t) > sizeof(uint32_t))
        assert(huffCountFrequencies(NULL, (size_t)UINT32_MAX + 1, frequencies) == -1);
    printf("--- testEmptyInput finished! ---\n");
}

static void testSampleText(void)
{
    printf("--- testSampleText started! ---\n");
    const char text[] = "ABRACADABRA";
    uint32_t frequencies[256];
    CodeTable table;
    HuffNode* root = NULL;

    assert(huffCountFrequencies((const uint8_t*)text, 11, frequencies) == 0);
    assert(frequencies[(unsigned char)'A'] == 5);
    assert(frequencies[(unsigned char)'B'] == 2);
    assert(frequencies[(unsigned char)'R'] == 2);
    assert(frequencies[(unsigned char)'C'] == 1);
    assert(frequencies[(unsigned char)'D'] == 1);
    assert(huffBuildTree(frequencies, &root) == 0);
    huffBuildCodeTable(root, table);

    assert(codeCost(table, frequencies) == 23);
    assertPrefixFree(table, frequencies);
    assert(huffCheckKraft(root) == 0);
    huffFreeTree(root);
    printf("--- testSampleText finished! ---\n");
}

// Тысяча одинаковых байтов: один реальный лист и фиктивный с весом 0
static void testSingleSymbol(void)
{
    printf("--- testSingleSymbol started! ---\n");
    uint8_t data[1000];
    uint32_t frequencies[256];
    CodeTable table;
    HuffNode* root = NULL;
    const HuffNode* left;
    const HuffNode* right;
    int symbol;

    for (symbol = 0; symbol < 1000; ++symbol)
        data[symbol] = 'a';

    assert(huffCountFrequencies(data, 1000, frequencies) == 0);
    assert(frequencies[(unsigned char)'a'] == 1000);
    for (symbol = 0; symbol < 256; ++symbol) {
        if (symbol != 'a')
            assert(frequencies[symbol] == 0);
    }

    assert(huffBuildTree(frequencies, &root) == 0);
    huffBuildCodeTable(root, table);
    assert(table[(unsigned char)'a'].length == 1);
    assert(huffComputeMaxCodeLength(root) == 1);
    assert(!huffIsLeaf(root));

    left = huffGetLeft(root);
    right = huffGetRight(root);
    assert(huffIsLeaf(left));
    assert(huffIsLeaf(right));
    assert(huffGetWeight(left) == 0);
    assert(huffGetSymbol(right) == 'a');
    assert(huffGetWeight(right) == 1000);
    assert(huffCheckKraft(root) == 0);
    huffFreeTree(root);
    printf("--- testSingleSymbol finished! ---\n");
}

// Каждый из 256 байтов встречается один раз
static void testUniformAlphabet(void)
{
    printf("--- testUniformAlphabet started! ---\n");
    uint8_t data[256];
    uint32_t frequencies[256];
    CodeTable table;
    HuffNode* root = NULL;
    int symbol;

    for (symbol = 0; symbol < 256; ++symbol)
        data[symbol] = (uint8_t)symbol;

    assert(huffCountFrequencies(data, 256, frequencies) == 0);
    assert(huffBuildTree(frequencies, &root) == 0);
    huffBuildCodeTable(root, table);

    for (symbol = 0; symbol < 256; ++symbol)
        assert(table[symbol].length == 8);
    assert(codeCost(table, frequencies) == 256 * 8);
    assert(huffComputeMaxCodeLength(root) == 8);
    assert(huffCheckKraft(root) == 0);
    assertPrefixFree(table, frequencies);
    huffFreeTree(root);
    printf("--- testUniformAlphabet finished! ---\n");
}

// Полное дерево из частот проходит проверку
static void testKraftAcceptsFullTree(void)
{
    printf("--- testKraftAcceptsFullTree started! ---\n");
    const char text[] = "AB";
    uint32_t frequencies[256];
    HuffNode* root = NULL;

    assert(huffCountFrequencies((const uint8_t*)text, 2, frequencies) == 0);
    assert(huffBuildTree(frequencies, &root) == 0);
    assert(huffCheckKraft(root) == 0);
    huffFreeTree(root);
    printf("--- testKraftAcceptsFullTree finished! ---\n");
}

// У корня только один ребёнок, равенство Крафта не выполняется
static void testKraftRejectsMissingLeaf(void)
{
    printf("--- testKraftRejectsMissingLeaf started! ---\n");
    HuffNode* leaf = huffCreateLeaf('A', 1, 1);
    HuffNode* root;

    assert(leaf != NULL);
    root = huffCreateParent(leaf, NULL);
    assert(root != NULL);
    assert(huffCheckKraft(root) == -1);
    huffFreeTree(root);
    printf("--- testKraftRejectsMissingLeaf finished! ---\n");
}

// Лист на глубине 16 длиннее разрешённых 15 бит
static void testKraftRejectsLongCode(void)
{
    printf("--- testKraftRejectsLongCode started! ---\n");
    HuffNode* node = huffCreateLeaf('A', 1, 1);
    int depth;

    assert(node != NULL);
    for (depth = 0; depth < 16; ++depth) {
        HuffNode* sibling = huffCreateLeaf((uint8_t)depth, 1, 1);
        assert(sibling != NULL);
        node = huffCreateParent(node, sibling);
        assert(node != NULL);
    }

    assert(huffComputeMaxCodeLength(node) == 16);
    assert(huffCheckKraft(node) == -1);
    huffFreeTree(node);
    printf("--- testKraftRejectsLongCode finished! ---\n");
}

// 1024 псевдослучайных байта: запись кодов и чтение по дереву дают исходный буфер
static void testEncodeAndDecode(void)
{
    printf("--- testEncodeAndDecode started! ---\n");
    const char* path = "huffmanEncodeAndDecode.bin";
    uint8_t data[1024];
    uint8_t decoded[1024];
    uint32_t frequencies[256];
    CodeTable table;
    HuffNode* root = NULL;
    FILE* file;
    BitWriter writer;
    BitReader reader;
    uint32_t state = 1;
    int index;

    for (index = 0; index < 1024; ++index) {
        state = state * 1664525 + 1013904223;
        data[index] = (uint8_t)(state >> 24);
    }

    assert(huffCountFrequencies(data, 1024, frequencies) == 0);
    assert(huffBuildTree(frequencies, &root) == 0);
    huffBuildCodeTable(root, table);

    file = fopen(path, "wb");
    assert(file != NULL);
    bitWriterInit(&writer, file);
    for (index = 0; index < 1024; ++index) {
        const Code* code = &table[data[index]];
        int bitIndex;
        for (bitIndex = 0; bitIndex < code->length; ++bitIndex) {
            uint32_t bit = (uint32_t)((code->code[bitIndex / 8] >> (7 - (bitIndex % 8))) & 1);
            bitWriterWriteBits(&writer, bit, 1);
        }
    }
    bitWriterFlush(&writer);
    fclose(file);

    file = fopen(path, "rb");
    assert(file != NULL);
    bitReaderInit(&reader, file);
    for (index = 0; index < 1024; ++index) {
        const HuffNode* node = root;
        while (!huffIsLeaf(node)) {
            uint32_t bit = 0;
            assert(bitReaderReadBits(&reader, 1, &bit) == 0);
            if (bit == 0)
                node = huffGetLeft(node);
            else
                node = huffGetRight(node);
            assert(node != NULL);
        }
        decoded[index] = huffGetSymbol(node);
    }
    fclose(file);
    remove(path);

    for (index = 0; index < 1024; ++index)
        assert(decoded[index] == data[index]);
    huffFreeTree(root);
    printf("--- testEncodeAndDecode finished! ---\n");
}

// Запускает проверки дерева, Крафта и кодирования/декодирования
int main(void)
{
    printf("--- testHuffman started! ---\n");
    testEmptyInput();
    testSampleText();
    testSingleSymbol();
    testUniformAlphabet();
    testKraftAcceptsFullTree();
    testKraftRejectsMissingLeaf();
    testKraftRejectsLongCode();
    testEncodeAndDecode();
    printf("--- testHuffman finished! ---\n");
    return 0;
}
