#pragma once

#include <stdio.h>

// Создаёт архив .huf из списка файлов
int archiveCreate(const char* archivePath, const char** inputFiles, size_t numberOfFiles);

// Извлекает архив в каталог
int archiveExtract(const char* archivePath, const char* outputDirectory);

// Печатает строки «исходный размер, сжатый размер, путь»
int archiveList(const char* archivePath, FILE* output);
