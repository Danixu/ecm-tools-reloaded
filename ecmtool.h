////////////////////////////////////////////////////////////////////////////////
//
#define TITLE "ecmtool - Encoder/decoder for Error Code Modeler format, with advanced features"
#define COPYR "Copyright (C) 2021 Daniel Carrasco"
#define VERSI "2.3.0-alpha"
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////

#include "banner.h"
#include "sector_tools.h"
#include <getopt.h>
#include <stdbool.h>
#include <string>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <vector>


// Configurations
#define SECTORS_PER_BLOCK 100
#define BUFFER_SIZE 0x500000lu

// MB Macro
#define MB(x) ((float)(x) / 1024 / 1024)

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

void printfileerror(FILE *f, const char *name) {
    printf("Error: ");
    if(name) { printf("%s: ", name); }
    printf("%s\n", f && feof(f) ? "Unexpected end-of-file" : strerror(errno));
}

////////////////////////////////////////////////////////////////////////////////

// Streams and sectors structs
#pragma pack(push, 1)
struct stream {
    uint8_t type : 1;
    uint8_t compression : 3;
    uint32_t end_sector = 0;
    uint32_t out_end_position = 0;
};

struct sector {
    uint8_t mode : 4;
    uint32_t sector_count = 0;
};
#pragma pack(pop)

struct sec_str_size {
    uint32_t count;
    uint32_t size;
};

// Struct for script vector
struct stream_script {
    stream stream_data;
    std::vector<sector> sectors_data;
};

// Ecmify options struct
struct ecm_options {
    bool force_rewrite = false;
    bool keep_output = false;
    sector_tools_compression data_compression = C_NONE;
    sector_tools_compression audio_compression = C_NONE;
    uint8_t compression_level = 5;
    bool extreme_compression = false;
    bool seekable = false;
    uint8_t sectors_per_block = SECTORS_PER_BLOCK;
    optimization_options optimizations = (
        OO_REMOVE_SYNC |
        OO_REMOVE_MSF |
        OO_REMOVE_MODE |
        OO_REMOVE_BLANKS |
        OO_REMOVE_REDUNDANT_FLAG |
        OO_REMOVE_ECC |
        OO_REMOVE_EDC |
        OO_REMOVE_GAP
    );
};

////////////////////////////////////////////////////////////////////////////////

// Return codes
enum ecmtool_return_code {
    ECMTOOL_OK = 0,
    ECMTOOL_FILE_READ_ERROR = INT_MIN,
    ECMTOOL_FILE_WRITE_ERROR,
    ECMTOOL_HEADER_COMPRESSION_ERROR,
    ECMTOOL_BUFFER_MEMORY_ERROR,
    ECMTOOL_PROCESSING_ERROR,
    ECMTOOL_CORRUPTED_STREAM,
    ECMTOOL_CORRUPTED_HEADER
};

////////////////////////////////////////////////////////////////////////////////

// Declare the functions
void print_help();
static ecmtool_return_code ecmify(
    const char *infilename,
    const char *outfilename,
    ecm_options *options
);
static ecmtool_return_code unecmify(
    const char *infilename,
    const char *outfilename,
    ecm_options *options
);
int compress_header (
    uint8_t *dest,
    uint32_t &destLen,
    uint8_t *source,
    uint32_t sourceLen,
    int level
);
int decompress_header (
    uint8_t *dest,
    uint32_t &destLen,
    uint8_t *source,
    uint32_t sourceLen
);
static ecmtool_return_code task_maker (
    stream *streams_toc,
    sec_str_size &streams_toc_count,
    sector *sectors_toc,
    sec_str_size &sectors_toc_count,
    std::vector<stream_script> &streams_script
);
static ecmtool_return_code disk_analyzer (
    sector_tools *sTools,
    FILE *image_file,
    size_t image_file_size,
    stream *streams_toc,
    sec_str_size *streams_toc_size,
    sector *sectors_toc,
    sec_str_size *sectors_toc_size,
    ecm_options *options
);
static ecmtool_return_code disk_encode (
    sector_tools *sTools,
    FILE *image_file,
    FILE *emc_out,
    std::vector<stream_script> &streams_script,
    ecm_options *options,
    uint32_t *sectors_type,
    uint32_t &input_edc
);
static ecmtool_return_code disk_decode (
    sector_tools *sTools,
    FILE *ecm_in,
    FILE *image_out,
    std::vector<stream_script> &streams_script,
    ecm_options *options
);
static void resetcounter(off_t total);
static void encode_progress(void);
static void decode_progress(void);
static void setcounter_analyze(off_t n);
static void setcounter_encode(off_t n);
static void setcounter_decode(off_t n);

static void summary (
    uint32_t *sectors,
    ecm_options *options,
    sector_tools *sTools,
    size_t compressed_size
);