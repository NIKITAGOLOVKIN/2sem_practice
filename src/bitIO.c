#include "bitIO.h"

void bitWriterInit(BitWriter* writer, FILE* file)
{
    writer->file = file;
    writer->buffer = 0;
    writer->bitCount = 0;
}

void bitWriterWriteBits(BitWriter* writer, uint32_t value, int numberOfBits)
{
    if (numberOfBits <= 0 || numberOfBits > 32)
        return;

    for (int i = numberOfBits - 1; i >= 0; --i) {
        uint32_t bit = (value >> i) & 1;
        writer->buffer = (uint8_t)((writer->buffer << 1) | bit);
        ++writer->bitCount;
        if (writer->bitCount == 8) {
            fputc(writer->buffer, writer->file);
            writer->buffer = 0;
            writer->bitCount = 0;
        }
    }
}

void bitWriterFlush(BitWriter* writer)
{
    if (writer->bitCount == 0)
        return;

    writer->buffer = (uint8_t)(writer->buffer << (8 - writer->bitCount));
    fputc(writer->buffer, writer->file);
    writer->buffer = 0;
    writer->bitCount = 0;
}

void bitReaderInit(BitReader* reader, FILE* file)
{
    reader->file = file;
    reader->buffer = 0;
    reader->bitCount = 0;
}

int bitReaderReadBits(BitReader* reader, int numberOfBits, uint32_t* output)
{
    uint32_t value = 0;

    if (numberOfBits < 0 || numberOfBits > 32)
        return -1;

    for (int i = 0; i < numberOfBits; ++i) {
        int byte;
        uint32_t bit;

        if (reader->bitCount == 0) {
            byte = fgetc(reader->file);
            if (byte == EOF)
                return -1;
            reader->buffer = (uint8_t)byte;
            reader->bitCount = 8;
        }

        bit = (uint32_t)((reader->buffer >> 7) & 1);
        reader->buffer = (uint8_t)(reader->buffer << 1);
        --(reader->bitCount);
        value = (value << 1) | bit;
    }

    *output = value;
    return 0;
}