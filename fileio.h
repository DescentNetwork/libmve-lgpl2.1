#ifndef MVE_FILE_IO_H
#define MVE_FILE_IO_H

#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>


void readptr(void* data, size_t count);
void skipnbytes(int count);

uint8_t read8(void);

uint16_t read16(void);
uint32_t read32(void);
uint64_t read64(void);

uint16_t read16LE(void);
uint32_t read32LE(void);
uint64_t read64LE(void);

#endif /* MVE_FILE_IO_H */
