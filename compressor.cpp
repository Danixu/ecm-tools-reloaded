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

#include <stdexcept>
#include "compressor.h"

compressor::compressor(sector_tools_compression mode, bool is_compression, int32_t comp_level) {
    comp_mode = mode;
    compression = is_compression;
    compression_level = comp_level;
    int ret;
    // Class initialzer
    switch(mode) {
    case C_ZLIB:
        strm_zlib.zalloc = Z_NULL;
        strm_zlib.zfree = Z_NULL;
        strm_zlib.opaque = Z_NULL;

        if (is_compression) {
            ret = deflateInit(&strm_zlib, comp_level);
        }
        else {
            ret = inflateInit(&strm_zlib);
        }
        
        if (ret != Z_OK) {
            //throw std::runtime_error("Error initializing the zlib compressor/decompressor.");
        }

        break;

    case C_LZMA:
        strm_lzma = LZMA_STREAM_INIT;

        if (is_compression) {
            lzma_lzma_preset(&opt_lzma2, comp_level);

            lzma_filter filters[] = {
                { LZMA_FILTER_X86, NULL },
                { LZMA_FILTER_LZMA2, &opt_lzma2 },
                { LZMA_VLI_UNKNOWN, NULL },
            };
            ret = lzma_stream_encoder(&strm_lzma, filters, LZMA_CHECK_NONE); // CRC is already checked

            // Return successfully if the initialization went fine.
            if (ret != LZMA_OK) {
                //throw std::runtime_error("Error initializing the lzma2 compressor/decompressor.");
            }
        }
        else {
            ret = lzma_stream_decoder(
                &strm_lzma,
                UINT64_MAX,
                LZMA_IGNORE_CHECK
            );

            // Return successfully if the initialization went fine.
            if (ret != LZMA_OK) {
                //throw std::runtime_error("Error initializing the lzma2 compressor/decompressor.");
            }
        }

        break;

    case C_LZ4:
        // We will create blocks of 200Kb which is near to output buffer (to keep it working correctly).
        lzlib4_compress_init(&strm_lz4, 204800, LZLIB4_INPUT_NOSPLIT, (int)(1.34 * comp_level));

        /*
        strm_lz4 = LZ4_createStreamHC();
        if (strm_lz4 == NULL) {
             //throw std::runtime_error("Error initializing the lz4 compressor/decompressor.");
        }
        //LZ4_resetStreamHC(strm_lz4, (int)(1.34 * compression_level));
        LZ4_resetStreamHC(strm_lz4, LZ4HC_CLEVEL_MAX);
        //LZ4_setCompressionLevel(strm_lz4, (int)(1.34 * comp_level));
        */
        break;
    }
}


int8_t compressor::set_input(uint8_t* in, size_t &in_size){
    if (!compression) {
        switch(comp_mode) {
        case C_ZLIB:
            strm_zlib.avail_in = in_size;
            strm_zlib.next_in = in;

            return 0;
            break;

        case C_LZMA:
            strm_lzma.avail_in = in_size;
            strm_lzma.next_in = in;

            return 0;
            break;

        case C_LZ4:
            strm_lz4.avail_in = in_size;
            strm_lz4.next_in = in;

            return 0;
            break;
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a decompression object to compress.");
        return -1;
    }
}


int8_t compressor::set_output(uint8_t* out, size_t &out_size){
    if (compression) {
        switch(comp_mode) {
        case C_ZLIB:
            strm_zlib.avail_out = out_size;
            strm_zlib.next_out = out;

            return 0;
            break;

        case C_LZMA:
            strm_lzma.avail_out = out_size;
            strm_lzma.next_out = out;

            return 0;
            break;

        case C_LZ4:
            strm_lz4.avail_out = out_size;
            strm_lz4.next_out = out;

            return 0;
            break;
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a decompression object to compress.");
        return -1;
    }
}


int8_t compressor::compress(size_t &out_size, uint8_t* in, size_t in_size, uint8_t flush_mode){
    if (compression) {
        int8_t return_code;
        lzma_action flushmode_lzma = LZMA_RUN;
        size_t processed;

        switch(comp_mode) {
        case C_ZLIB:
            strm_zlib.avail_in = in_size;
            strm_zlib.next_in = in;

            return_code = deflate(&strm_zlib, flush_mode);

            out_size = strm_zlib.avail_out;
            return return_code;
            break;

        case C_LZMA:
            strm_lzma.avail_in = in_size;
            strm_lzma.next_in = in;

            switch (flush_mode) {
                case Z_FULL_FLUSH:
                    flushmode_lzma = LZMA_FULL_FLUSH;
                    break;

                case Z_FINISH:
                    flushmode_lzma = LZMA_FINISH;
                    break;
            }

            return_code = lzma_code(&strm_lzma, flushmode_lzma);
            
            if (return_code == LZMA_STREAM_END) {
                return LZMA_OK;
            }

            out_size = strm_lzma.avail_out;
            return return_code;
            break;

        case C_LZ4:
            strm_lz4.avail_in = in_size;
            strm_lz4.next_in = in;

            lzlib4_compress_block(&strm_lz4, (lzlib4_flush_mode)flush_mode);

            out_size = strm_lz4.avail_out;
            
            /*
            processed = LZ4_compress_HC_continue(strm_lz4, (const char *)strm_lzma.next_in, (char*)strm_lzma.next_out, strm_lzma.avail_in, strm_lzma.avail_out);

            if (processed == 0) {
                // Surelly an error ocurred
                return 1;
            }
            // We calculate how much space left in buffer and set it into out_size
            strm_lzma.avail_out -= processed;
            out_size = strm_lzma.avail_out;

            // Set the new pointer position
            strm_lzma.next_out += processed;
            // This compressor doesn't set the input buffer space it left, so must be set manually
            strm_lzma.avail_in = 0;
            if (flushmode == Z_FINISH) {
                LZ4_resetStreamHC(strm_lz4, (int)(1.34 * compression_level));
            }
            */
            break;
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a decompression object to compress.");
        return -1;
    }
}

int8_t compressor::decompress(uint8_t* out, size_t out_size, size_t &in_size, uint8_t flusmode){
    if (!compression) {
        int8_t return_code;
        switch(comp_mode) {
        case C_ZLIB:
            strm_zlib.avail_out = out_size;
            strm_zlib.next_out = out;
            return_code = inflate(&strm_zlib, flusmode);

            in_size = strm_zlib.avail_in;
            return return_code;
            break;

        case C_LZMA:
            strm_lzma.avail_out = out_size;
            strm_lzma.next_out = out;
            return_code = lzma_code(&strm_lzma, LZMA_RUN);

            in_size = strm_lzma.avail_in;
            return return_code;
            break;
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a compression object to decompress.");
        return -1;
    }
}

int8_t compressor::close(){
    switch(comp_mode) {
    case C_ZLIB:
        if (compression) {
            (void)deflateEnd(&strm_zlib);
            strm_zlib = {};
        }
        else {
            (void)inflateEnd(&strm_zlib);
            strm_zlib = {};
        }
        return 0;
        break;

    case C_LZMA:
        lzma_end(&strm_lzma);
        strm_lzma = {};
        return 0;
        break;

    case C_LZ4:
        //LZ4_freeStreamHC(&strm_lz4); // Closes the program when used. Must be investigated.
        return 0;
        break;
    }

    return -1;
}

size_t compressor::data_left_in() {
    switch(comp_mode) {
    case C_ZLIB:
        return strm_zlib.avail_in;
        break;

    case C_LZMA:
    case C_LZ4:
        return strm_lzma.avail_in;
        break;
    }

    return -1;
};

size_t compressor::data_left_out() {
    switch(comp_mode) {
    case C_ZLIB:
        return strm_zlib.avail_out;
        break;

    case C_LZMA:
    case C_LZ4:
        return strm_lzma.avail_out;
        break;
    }

    return -1;
};