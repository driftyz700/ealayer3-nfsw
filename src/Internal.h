/*
    EA Layer 3 Extractor/Decoder
    Copyright (C) 2010, Ben Moench.
    See License.txt
*/

#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>

#include "MyStdInt.h"

#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <exception>

#include <boost/smart_ptr.hpp>
#include <boost/make_shared.hpp>
using namespace boost;

//#define ENABLE_VERY_VERBOSE

// Macro for verbose
extern int g_Verbose;
#define VERBOSE(_output) VERBOSE_NO_ENDL(_output << std::endl)
#define VERBOSE_NO_ENDL(_output) if(g_Verbose >= 1) { std::cout << _output; }
#define VERBOSEVAR(_variable) VERBOSE("    " << #_variable << " = " << (_variable))

// Macro for very verbose
#ifdef ENABLE_VERY_VERBOSE
#define VERY_VERBOSE(_output) VERY_VERBOSE_NO_ENDL(_output << std::endl)
#define VERY_VERBOSE_NO_ENDL(_output) if(g_Verbose >= 2) { std::cout << _output; }
#else
#define VERY_VERBOSE(_output)
#define VERY_VERBOSE_NO_ENDL(_output)
#endif

#ifndef NULL
#define NULL 0
#endif // NULL

#define ENCODER_UNCOM_SAMPLES 47
