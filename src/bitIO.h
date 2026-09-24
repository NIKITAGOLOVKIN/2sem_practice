#pragma once

#include <stdint.h>
#include <stdio.h>

// Структура для записи битов
typedef struct BitWriter {
    FILE* file; // Файл для записи
    uint8_t buffer; // Буфер для записи
    int bitCount; // Количество битов в буфере
} BitWriter;

// Структура для чтения битов
typedef struct BitReader {
    FILE* file; // Файл для чтения
    uint8_t buffer; // Буфер для чтения
    int bitCount; // Количество битов в буфере
} BitReader;

// Инициализирует писателя битов
void bitWriterInit(BitWriter* writer, FILE* file);

// Записывает numberOfBits битов из value в буфер writer,
// если буфер заполнен, то записывает его в файл
void bitWriterWriteBits(BitWriter* writer, uint32_t value, int numberOfBits);

// Записывает остаток буфера writer в файл
void bitWriterFlush(BitWriter* writer);

// Инициализирует читателя битов
void bitReaderInit(BitReader* reader, FILE* file);

// Читает до numberOfBits битов из буфера reader и сохраняет результат в output
int bitReaderReadBits(BitReader* reader, int numberOfBits, uint32_t* output);