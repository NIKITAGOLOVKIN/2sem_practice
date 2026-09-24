#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct HuffNode HuffNode;

// Элемент кучи: узел дерева и его вес
typedef struct PriorityQueueItem {
    HuffNode* node; // Узел дерева
    uint64_t weight; // Вес узла
} PriorityQueueItem;

// Минимальная куча на динамическом массиве
typedef struct PriorityQueue {
    PriorityQueueItem* items; // Массив элементов кучи
    size_t size; // Количество элементов в куче
    size_t capacity; // Вместимость кучи
} PriorityQueue;

// Готовит пустую очередь
void priorityQueueInit(PriorityQueue* queue);

// Кладёт узел в очередь
int priorityQueuePush(PriorityQueue* queue, HuffNode* node, uint32_t weight);

// Забирает узел с наименьшим весом
int priorityQueuePop(PriorityQueue* queue, HuffNode** outputNode, uint32_t* outputWeight);

// Сколько узлов сейчас в очереди
size_t priorityQueueSize(const PriorityQueue* queue);

// Освобождает массив очереди, сами узлы не удаляет
void priorityQueueFree(PriorityQueue* queue);
