/*******************************************************************************
 */
#define TITLE "ecmtool - Encoder/decoder for Error Code Modeler format, with advanced features"
#define COPYR "Copyright (C) 2021 Daniel Carrasco"
#define VERSI "3.0.0-alpha"
/* 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "banner.h"
#include "sector_tools.h"
#include <getopt.h>
//#include <stdbool.h>
#include <algorithm>
//#include <stdexcept>
#include <stdio.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <time.h>
//#include <limits.h>
//#include <ctype.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <chrono>


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

struct block_header {
    uint8_t type = 0;
    uint8_t compression = 0;
    uint64_t block_size = 0;
    uint64_t real_block_size = 0;
};

struct blocks_toc {
    uint8_t type;
    uint64_t start_position;
};

struct ecm_header {
    uint8_t optimizations;
    uint8_t sectors_per_block;
    uint64_t crc_mode;
    uint64_t streams_toc_pos;
    uint64_t sectors_toc_pos;
    uint64_t ecm_data_pos;
    uint8_t title_length;
    uint8_t id_length;
    std::string title;
    std::string id;
};

struct sec_str_size {
    sector_tools_compression compression;
    uint32_t count;
    uint32_t uncompressed_size;
    uint32_t compressed_size;
};
#pragma pack(pop)

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
    std::string in_filename;
    std::string out_filename;
    std::string image_title;
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

enum ecmfile_block_type {
    ECMFILE_BLOCK_TYPE_DELETED = 0,
    ECMFILE_BLOCK_TYPE_METADATA,
    ECMFILE_BLOCK_TYPE_TOC,
    ECMFILE_BLOCK_TYPE_ECM,
    ECMFILE_BLOCK_TYPE_FILE,
};

////////////////////////////////////////////////////////////////////////////////

// Declare the functions
void print_help();
int get_options(
    int argc,
    char **argv,
    ecm_options *options
);
int image_to_ecm_block(
    std::ifstream &in_file,
    std::fstream &out_file,
    ecm_options *options,
    std::vector<uint32_t> *sectors_type_sumary
);
int ecm_block_to_image(
    std::ifstream &in_file,
    std::fstream &out_file,
    ecm_options *options
);
int write_block_header(
    std::fstream &out_file,
    block_header *block_header
);
int read_block_header(
    std::ifstream &out_file,
    block_header *block_header
);
static ecmtool_return_code disk_analyzer (
    sector_tools *sTools,
    std::ifstream &in_file,
    size_t image_file_size,
    std::vector<stream_script> &streams_script,
    ecm_header *ecm_data_header,
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
static ecmtool_return_code task_to_streams_header (
    stream *&streams_toc,
    sec_str_size &streams_toc_count,
    std::vector<stream_script> &streams_script
);
static ecmtool_return_code task_to_sectors_header (
    sector *&sectors_toc,
    sec_str_size &sectors_toc_count,
    std::vector<stream_script> &streams_script
);

static ecmtool_return_code disk_encode (
    sector_tools *sTools,
    std::ifstream &in_file,
    std::fstream &out_file,
    std::vector<stream_script> &streams_script,
    ecm_options *options,
    std::vector<uint32_t> *sectors_type,
    uint64_t ecm_block_start_position
);
static ecmtool_return_code disk_decode (
    sector_tools *sTools,
    std::ifstream &in_file,
    std::fstream &out_file,
    std::vector<stream_script> &streams_script,
    ecm_options *options,
    uint64_t ecm_block_start_position
);
static void resetcounter(uint64_t total);
static void encode_progress(void);
static void decode_progress(void);
static void setcounter_analyze(uint64_t n);
static void setcounter_encode(uint64_t n);
static void setcounter_decode(uint64_t n);

static void summary (
    std::vector<uint32_t> *sectors_type,
    ecm_options *options,
    size_t compressed_size
);

void print_task(
    std::vector<stream_script> &streams_script
);

int detect_id_psx(std::string &id, uint8_t *data, uint64_t data_size);

/*
void write_to_file(std::string filename, uint8_t *data, uint64_t size) {
    FILE *out_file = fopen(filename.c_str(), "wb");
    fwrite(data, size, 1, out_file);
    fclose(out_file);
}
*/