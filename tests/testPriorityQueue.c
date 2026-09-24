#include "priorityQueue.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Следующий вес из фиксированной псевдослучайной последовательности
static uint32_t nextWeight(uint32_t* state)
{
    *state = *state * 1664525 + 1013904223;
    return *state;
}

// Сравнивает два веса для сортировки ожидаемого порядка
static int compareWeights(const void* leftPointer, const void* rightPointer)
{
    uint32_t leftWeight = *(const uint32_t*)leftPointer;
    uint32_t rightWeight = *(const uint32_t*)rightPointer;

    if (leftWeight < rightWeight)
        return -1;
    if (leftWeight > rightWeight)
        return 1;
    return 0;
}

// Пустая очередь возвращает ошибку и не портит выходные переменные
static void testEmptyQueue(void)
{
    printf("--- testEmptyQueue started! ---\n");
    PriorityQueue queue;
    HuffNode* outputNode = (HuffNode*)(uintptr_t)1;
    uint32_t outputWeight = 42;

    priorityQueueInit(&queue);
    assert(priorityQueueSize(&queue) == 0);
    assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == -1);
    assert(outputNode == (HuffNode*)(uintptr_t)1);
    assert(outputWeight == 42);
    priorityQueueFree(&queue);
    printf("--- testEmptyQueue finished! ---\n");
}

// Несколько известных весов выходят от меньшего к большему
static void testKnownOrder(void)
{
    printf("--- testKnownOrder started! ---\n");
    PriorityQueue queue;
    HuffNode* lightNode = (HuffNode*)(uintptr_t)1;
    HuffNode* middleNode = (HuffNode*)(uintptr_t)3;
    HuffNode* heavyNode = (HuffNode*)(uintptr_t)5;
    HuffNode* outputNode = NULL;
    uint32_t outputWeight = 0;

    priorityQueueInit(&queue);
    assert(priorityQueuePush(&queue, heavyNode, 5) == 0);
    assert(priorityQueuePush(&queue, lightNode, 1) == 0);
    assert(priorityQueuePush(&queue, middleNode, 3) == 0);
    assert(priorityQueueSize(&queue) == 3);

    assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == 0);
    assert(outputNode == lightNode);
    assert(outputWeight == 1);

    assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == 0);
    assert(outputNode == middleNode);
    assert(outputWeight == 3);

    assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == 0);
    assert(outputNode == heavyNode);
    assert(outputWeight == 5);

    assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == -1);
    priorityQueueFree(&queue);
    printf("--- testKnownOrder finished! ---\n");
}

static void testManyWeights(void)
{
    printf("--- testManyWeights started! ---\n");
    PriorityQueue queue;
    uint32_t weights[256];
    uint32_t expected[256];
    uint32_t state = 1;
    uint32_t previousWeight = 0;
    HuffNode* outputNode = NULL;
    uint32_t outputWeight = 0;

    priorityQueueInit(&queue);
    for (int i = 0; i < 256; ++i) {
        weights[i] = nextWeight(&state);
        expected[i] = weights[i];
        assert(priorityQueuePush(&queue, (HuffNode*)(uintptr_t)(i + 1), weights[i]) == 0);
    }
    assert(priorityQueueSize(&queue) == 256);

    qsort(expected, 256, sizeof(expected[0]), compareWeights);
    for (int i = 0; i < 256; ++i) {
        assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == 0);
        assert(outputWeight == expected[i]);
        assert(previousWeight <= outputWeight);
        previousWeight = outputWeight;
    }

    assert(priorityQueueSize(&queue) == 0);
    assert(priorityQueuePop(&queue, &outputNode, &outputWeight) == -1);
    priorityQueueFree(&queue);
    printf("--- testManyWeights finished! ---\n");
}

int main(void)
{
    printf("--- testPriorityQueue started! ---\n");
    testEmptyQueue();
    testKnownOrder();
    testManyWeights();
    printf("--- testPriorityQueue finished! ---\n");
    return 0;
}
