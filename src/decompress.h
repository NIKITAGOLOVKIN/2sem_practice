#pragma once

#include <stdint.h>
#include <stdio.h>

// Восстанавливает файл из потока битов и таблицы частот
int decompressStream(FILE* input, FILE* output, uint64_t originalSize, const uint32_t frequencies[static 256]);