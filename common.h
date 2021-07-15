#ifndef __ECMRELOADED_COMMON_H__
#define __ECMRELOADED_COMMON_H__

////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////
//
// Try to figure out integer types
//
#if defined(_STDINT_H) || defined(_EXACT_WIDTH_INTS)

// _STDINT_H_ - presume stdint.h has already been included
// _EXACT_WIDTH_INTS - OpenWatcom already provides int*_t in sys/types.h

#elif defined(__STDC__) && __STDC__ && __STDC_VERSION__ >= 199901L

// Assume C99 compliance when the compiler specifically tells us it is
#include <stdint.h>

#elif defined(_MSC_VER)

// On Visual Studio, use its integral types
typedef   signed __int8   int8_t;
typedef unsigned __int8  uint8_t;
typedef   signed __int16  int16_t;
typedef unsigned __int16 uint16_t;
typedef   signed __int32  int32_t;
typedef unsigned __int32 uint32_t;

#endif

////////////////////////////////////////////////////////////////////////////////
//
// Figure out how big file offsets should be
//
#if defined(_OFF64_T_) || defined(_OFF64_T_DEFINED) || defined(__off64_t_defined)
//
// We have off64_t
// Regular off_t may be smaller, so check this first
//

#ifdef off_t
#undef off_t
#endif
#ifdef fseeko
#undef fseeko
#endif
#ifdef ftello
#undef ftello
#endif

#define off_t off64_t
#define fseeko fseeko64
#define ftello ftello64

#elif defined(_OFF_T) || defined(__OFF_T_TYPE) || defined(__off_t_defined) || defined(_OFF_T_DEFINED_)
//
// We have off_t
//

#else
//
// Assume offsets are just 'long'
//
#ifdef off_t
#undef off_t
#endif
#ifdef fseeko
#undef fseeko
#endif
#ifdef ftello
#undef ftello
#endif

#define off_t long
#define fseeko fseek
#define ftello ftell

#endif

////////////////////////////////////////////////////////////////////////////////

void printfileerror(FILE* f, const char* name) {
    printf("Error: ");
    if(name) { printf("%s: ", name); }
    printf("%s\n", f && feof(f) ? "Unexpected end-of-file" : strerror(errno));
}

#endif
