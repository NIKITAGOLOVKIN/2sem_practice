#pragma once

#include <stdint.h>
#include <stdio.h>

// Сжимает один файл в поток битов
int compressStream(FILE* input, FILE* output, uint64_t* originalSize, uint64_t* compressedSize, uint32_t frequencies[static 256]);