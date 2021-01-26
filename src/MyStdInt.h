/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#ifdef _WIN32

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long ssize_t;

#else
#include <stdint.h>
#endif

