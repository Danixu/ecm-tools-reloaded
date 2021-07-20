////////////////////////////////////////////////////////////////////////////////
//
// Created by Daniel Carrasco at https://www.electrosoftcloud.com
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

#define LZ4_HC_STATIC_LINKING_ONLY

#include <stdint.h>
#include "zlib.h"
#include "lzma.h"
#include "lz4hc.h"
#include "lzlib4.h"

//
// Stream types detectable by the class
//
enum sector_tools_compression : uint8_t {
    C_NONE = 0,
    C_ZLIB,
    C_LZMA,
    C_LZ4,
    C_FLAC,
    C_APE,
    C_WAVPACK,
};
    
//
// compressor Class
//
class compressor {
    public:
    // Public methods
        compressor(sector_tools_compression mode, bool is_compression, int32_t comp_level = 5);

        int8_t set_input(uint8_t* in, size_t &in_size);
        int8_t set_output(uint8_t* out, size_t &out_size);
        int8_t compress(size_t &out_size, uint8_t* in, size_t in_size, uint8_t flushmode = Z_NO_FLUSH);
        int8_t decompress(uint8_t* out, size_t out_size, size_t &in_size, uint8_t flusmode);
        size_t data_left_in();
        size_t data_left_out();

        int8_t close();

    private:
        // zlib object
        z_stream strm_zlib;
        lzma_stream strm_lzma;
        lzma_options_lzma opt_lzma2;
        lzlib4 * strm_lz4 = NULL;
        sector_tools_compression comp_mode;
        bool compression;
        int32_t compression_level;
};