#include "priorityQueue.h"
#include <stdlib.h>

static void swapItems(PriorityQueueItem* left, PriorityQueueItem* right)
{
    PriorityQueueItem temp = *left;
    *left = *right;
    *right = temp;
}

static void siftUp(PriorityQueue* queue, size_t index)
{
    while (index > 0) {
        size_t parentIndex = (index - 1) / 2;

        if (queue->items[parentIndex].weight <= queue->items[index].weight)
            break;

        swapItems(&queue->items[parentIndex], &queue->items[index]);
        index = parentIndex;
    }
}

static void siftDown(PriorityQueue* queue, size_t index)
{
    while (1) {
        size_t leftIndex = index * 2 + 1;
        size_t rightIndex = leftIndex + 1;
        size_t smallestIndex = index;

        if (leftIndex < queue->size
            && queue->items[leftIndex].weight < queue->items[smallestIndex].weight)
            smallestIndex = leftIndex;

        if (rightIndex < queue->size
            && queue->items[rightIndex].weight < queue->items[smallestIndex].weight)
            smallestIndex = rightIndex;

        if (smallestIndex == index)
            break;

        swapItems(&queue->items[index], &queue->items[smallestIndex]);
        index = smallestIndex;
    }
}

static int growQueue(PriorityQueue* queue)
{
    size_t newCapacity = queue->capacity == 0 ? 8 : queue->capacity * 2;
    PriorityQueueItem* newItems;

    newItems = realloc(queue->items, newCapacity * sizeof(*newItems));
    if (newItems == NULL)
        return -1;

    queue->items = newItems;
    queue->capacity = newCapacity;
    return 0;
}

void priorityQueueInit(PriorityQueue* queue)
{
    queue->items = NULL;
    queue->size = 0;
    queue->capacity = 0;
}

int priorityQueuePush(PriorityQueue* queue, HuffNode* node, uint32_t weight)
{
    if (queue->size == queue->capacity && growQueue(queue) != 0)
        return -1;

    queue->items[queue->size].node = node;
    queue->items[queue->size].weight = weight;
    queue->size = queue->size + 1;
    siftUp(queue, queue->size - 1);
    return 0;
}

int priorityQueuePop(PriorityQueue* queue, HuffNode** outputNode, uint32_t* outputWeight)
{
    if (queue->size == 0)
        return -1;

    PriorityQueueItem min = queue->items[0];
    queue->size = queue->size - 1;
    if (queue->size > 0) {
        queue->items[0] = queue->items[queue->size];
        siftDown(queue, 0);
    }

    *outputNode = min.node;
    *outputWeight = (uint32_t)min.weight;
    return 0;
}

size_t priorityQueueSize(const PriorityQueue* queue)
{
    return queue->size;
}

void priorityQueueFree(PriorityQueue* queue)
{
    free(queue->items);
    queue->items = NULL;
    queue->size = 0;
    queue->capacity = 0;
}
