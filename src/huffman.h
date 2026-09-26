#pragma once

#include "priorityQueue.h"
#include <stdint.h>

// Код одного символа
typedef struct Code {
    uint8_t code[32]; // Код символа
    int length; // Длина кода
} Code;

typedef Code CodeTable[256]; // Таблица кодов

// Лист
HuffNode* huffCreateLeaf(uint8_t symbol, uint32_t weight, int hasSymbol);

// Внутренний узел. Ребёнок может быть NULL, так тест собирает неполное дерево
HuffNode* huffCreateParent(HuffNode* left, HuffNode* right);

// 1, если узел — лист
int huffIsLeaf(const HuffNode* node);

// Взять байт, записанный в листе
uint8_t huffGetSymbol(const HuffNode* node);

// Взять вес узла
uint32_t huffGetWeight(const HuffNode* node);

// Взять левого ребёнка
const HuffNode* huffGetLeft(const HuffNode* node);

// Взять правого ребёнка
const HuffNode* huffGetRight(const HuffNode* node);

// Считает частоты байтов
int huffCountFrequencies(const uint8_t* data, size_t dataLength, uint32_t frequencies[256]);

// Строит дерево
int huffBuildTree(const uint32_t frequencies[static 256], HuffNode** outputRoot);

// Заполняет коды символов обходом дерева. Левая ветка — бит 0, правая — бит 1
void huffBuildCodeTable(const HuffNode* root, CodeTable outputTable);

// Самая длинная цепочка от корня до листа
int huffComputeMaxCodeLength(const HuffNode* root);

// Проверить условие Крафта
int huffCheckKraft(const HuffNode* root);

// Удаляет дерево
void huffFreeTree(HuffNode* root);
