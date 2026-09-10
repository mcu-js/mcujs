#ifndef SD_TEST_FF_H
#define SD_TEST_FF_H
#include <stdint.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;
#ifdef SD_TEST_LBA64
typedef uint64_t LBA_t;
#else
typedef uint32_t LBA_t;
#endif
#endif
