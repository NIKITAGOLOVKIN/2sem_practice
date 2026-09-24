#include "huffman.h"
#include <stdlib.h>

// Максимальная длина кода
enum {
    MAXIMUM_CODE_LENGTH = 15
};

// Узел дерева
struct HuffNode {
    struct HuffNode* left; // Левое поддерево
    struct HuffNode* right; // Правое поддерево
    uint32_t weight; // Вес узла
    uint8_t symbol; // Символ
    int isLeaf; // Флаг листа
    int hasSymbol; // Флаг наличия символа
};

// Освобождает очередь
static void discardQueue(PriorityQueue* queue)
{
    while (priorityQueueSize(queue) > 0) {
        HuffNode* node = NULL;
        uint32_t weight = 0;
        priorityQueuePop(queue, &node, &weight);
        huffFreeTree(node);
    }
    priorityQueueFree(queue);
}

// Ставит бит
static void appendBit(uint8_t* bytes, int length, int bit)
{
    if (bit)
        bytes[length / 8] = (uint8_t)(bytes[length / 8] | (uint8_t)(1 << (7 - (length % 8))));
}

// Удаляет бит
static void removeBit(uint8_t* bytes, int length)
{
    int bitIndex = length - 1;
    uint8_t mask = (uint8_t)(1 << (7 - (bitIndex % 8)));
    bytes[bitIndex / 8] = (uint8_t)(bytes[bitIndex / 8] & (uint8_t)~mask);
}

// Заполняет таблицу кодов
static void fillCodeTable(const HuffNode* node, Code* table, uint8_t* bytes, int length)
{
    if (length > 32 * 8)
        return;

    if (node->isLeaf) {
        if (!node->hasSymbol)
            return;
        table[node->symbol].length = length;
        for (int i = 0; i < 32; ++i)
            table[node->symbol].code[i] = bytes[i];
        return;
    }

    if (node->left != NULL) {
        appendBit(bytes, length, 0);
        fillCodeTable(node->left, table, bytes, length + 1);
        removeBit(bytes, length + 1);
    }
    if (node->right != NULL) {
        appendBit(bytes, length, 1);
        fillCodeTable(node->right, table, bytes, length + 1);
        removeBit(bytes, length + 1);
    }
}

static int leafDepth(const HuffNode* node, int depth)
{
    if (node == NULL)
        return 0;
    if (node->isLeaf)
        return depth;

    int leftDepth = leafDepth(node->left, depth + 1);
    int rightDepth = leafDepth(node->right, depth + 1);
    if (leftDepth > rightDepth)
        return leftDepth;
    return rightDepth;
}

static int accumulateKraft(const HuffNode* node, int depth, uint32_t* sum)
{
    if (node == NULL)
        return 0;

    if (node->isLeaf) {
        if (depth < 1 || depth > MAXIMUM_CODE_LENGTH)
            return -1;
        *sum += (uint32_t)1 << (MAXIMUM_CODE_LENGTH - depth);
        return 0;
    }

    if (accumulateKraft(node->left, depth + 1, sum) != 0)
        return -1;
    return accumulateKraft(node->right, depth + 1, sum);
}

// Создает лист
HuffNode* huffCreateLeaf(uint8_t symbol, uint32_t weight, int hasSymbol)
{
    HuffNode* node = calloc(1, sizeof(*node));
    if (node == NULL)
        return NULL;

    node->symbol = symbol;
    node->weight = weight;
    node->isLeaf = 1;
    node->hasSymbol = hasSymbol;
    return node;
}

// Создает родительский узел
HuffNode* huffCreateParent(HuffNode* left, HuffNode* right)
{
    HuffNode* node = calloc(1, sizeof(*node));
    uint32_t leftWeight = 0;
    uint32_t rightWeight = 0;

    if (node == NULL)
        return NULL;

    if (left != NULL)
        leftWeight = left->weight;
    if (right != NULL)
        rightWeight = right->weight;

    node->left = left;
    node->right = right;
    node->weight = leftWeight + rightWeight;
    node->isLeaf = 0;
    node->hasSymbol = 0;
    return node;
}

int huffIsLeaf(const HuffNode* node)
{
    return node->isLeaf;
}

uint8_t huffGetSymbol(const HuffNode* node)
{
    return node->symbol;
}

uint32_t huffGetWeight(const HuffNode* node)
{
    return node->weight;
}

const HuffNode* huffGetLeft(const HuffNode* node)
{
    return node->left;
}

const HuffNode* huffGetRight(const HuffNode* node)
{
    return node->right;
}

// Считает частоты байтов
int huffCountFrequencies(const uint8_t* data, size_t dataLength, uint32_t frequencies[static 256])
{
    if (dataLength > UINT32_MAX)
        return -1;

    for (size_t i = 0; i < 256; ++i)
        frequencies[i] = 0;
    for (size_t i = 0; i < dataLength; ++i)
        frequencies[data[i]] += 1;
    return 0;
}

// Cтроит дерево
int huffBuildTree(const uint32_t frequencies[static 256], HuffNode** outputRoot)
{
    PriorityQueue queue;
    int symbolCount = 0;
    int symbol;

    priorityQueueInit(&queue);
    for (symbol = 0; symbol < 256; ++symbol) {
        HuffNode* leaf;

        if (frequencies[symbol] == 0)
            continue;

        leaf = huffCreateLeaf((uint8_t)symbol, frequencies[symbol], 1);
        if (leaf == NULL || priorityQueuePush(&queue, leaf, frequencies[symbol]) != 0) {
            huffFreeTree(leaf);
            discardQueue(&queue);
            return -1;
        }
        symbolCount = symbolCount + 1;
    }

    if (symbolCount == 0) {
        priorityQueueFree(&queue);
        *outputRoot = NULL;
        return 0;
    }

    if (symbolCount == 1) {
        HuffNode* dummy = huffCreateLeaf(0, 0, 0);
        if (dummy == NULL || priorityQueuePush(&queue, dummy, 0) != 0) {
            huffFreeTree(dummy);
            discardQueue(&queue);
            return -1;
        }
    }

    while (priorityQueueSize(&queue) > 1) {
        HuffNode* left = NULL;
        HuffNode* right = NULL;
        HuffNode* parent;
        uint32_t leftWeight = 0;
        uint32_t rightWeight = 0;

        if (priorityQueuePop(&queue, &left, &leftWeight) != 0
            || priorityQueuePop(&queue, &right, &rightWeight) != 0) {
            huffFreeTree(left);
            huffFreeTree(right);
            discardQueue(&queue);
            return -1;
        }

        parent = huffCreateParent(left, right);
        if (parent == NULL || priorityQueuePush(&queue, parent, parent->weight) != 0) {
            if (parent == NULL) {
                huffFreeTree(left);
                huffFreeTree(right);
            } else {
                huffFreeTree(parent);
            }
            discardQueue(&queue);
            return -1;
        }
    }

    {
        uint32_t rootWeight = 0;
        if (priorityQueuePop(&queue, outputRoot, &rootWeight) != 0) {
            discardQueue(&queue);
            return -1;
        }
    }

    priorityQueueFree(&queue);
    return 0;
}

// Заполняет таблицу кодов
void huffBuildCodeTable(const HuffNode* root, CodeTable outputTable)
{
    uint8_t bytes[32];
    int symbol;
    int i;

    for (symbol = 0; symbol < 256; ++symbol) {
        outputTable[symbol].length = 0;
        for (i = 0; i < 32; ++i)
            outputTable[symbol].code[i] = 0;
    }
    for (i = 0; i < 32; ++i)
        bytes[i] = 0;

    if (root == NULL)
        return;
    fillCodeTable(root, outputTable, bytes, 0);
}

int huffComputeMaxCodeLength(const HuffNode* root)
{
    if (root == NULL)
        return 0;
    return leafDepth(root, 0);
}

int huffCheckKraft(const HuffNode* root)
{
    uint32_t sum = 0;

    if (root == NULL)
        return -1;
    if (accumulateKraft(root, 0, &sum) != 0)
        return -1;
    if (sum != ((uint32_t)1 << MAXIMUM_CODE_LENGTH))
        return -1;
    return 0;
}

void huffFreeTree(HuffNode* root)
{
    if (root == NULL)
        return;
    huffFreeTree(root->left);
    huffFreeTree(root->right);
    free(root);
}
