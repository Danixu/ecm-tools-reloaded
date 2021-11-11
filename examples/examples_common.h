/*******************************************************************************
 * 
 * Created by Daniel Carrasco at https://www.electrosoftcloud.com
 * 
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

#include "../sector_tools.h"

#include <getopt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>

#define ECM_FILE_VERSION 3

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
struct toc_file_header {
    uint64_t image_position = 0;
    uint64_t image_filesize = 0;
    optimization_options optimizations = OO_NONE;
    uint32_t image_crc = 0;
};

struct toc_file_sector {
    uint32_t image_relative_position = 0;
    uint32_t block_relative_position = 0;
    sector_tools_types sector_type;
    sector_tools_compression compression;
};

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
    bool seekable = false;
    uint8_t sectors_per_block = 0;
    std::vector<uint64_t> sector_to_read;
    uint32_t random = 0;
    std::string in_filename;
    std::string in_filename_toc;
    bool ecm_file = false;
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


void print_help();

int get_options(
    int argc,
    char **argv,
    ecm_options *options
);

int read_block_header(
    std::ifstream &in_file,
    block_header *block_header_data
);

int decompress_header (
    uint8_t *dest,
    uint32_t &destLen,
    uint8_t *source,
    uint32_t sourceLen
);