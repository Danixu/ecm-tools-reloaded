#include <stdexcept>
#include "compressor.h"

compressor::compressor(sector_tools_compression mode, bool is_compression, int8_t comp_level) {
    comp_mode = mode;
    compression = is_compression;
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
            lzma_lzma_preset(&opt_lzma2, 9);

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
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a decompression object to compress.");
        return -1;
    }
}


int8_t compressor::compress(size_t &out_size, uint8_t* in, size_t in_size, uint8_t flushmode){
    if (compression) {
        int8_t return_code;

        switch(comp_mode) {
        case C_ZLIB:
            strm_zlib.avail_in = in_size;
            strm_zlib.next_in = in;

            return_code = deflate(&strm_zlib, flushmode);

            out_size = strm_zlib.avail_out;
            return return_code;
            break;

        case C_LZMA:
            strm_lzma.avail_in = in_size;
            strm_lzma.next_in = in;

            lzma_action flushmode_lzma = LZMA_RUN;
            switch (flushmode) {
                case Z_FULL_FLUSH:
                    flushmode_lzma = LZMA_FULL_FLUSH;
                    break;

                case Z_FINISH:
                    flushmode_lzma = LZMA_FINISH;
                    break;
            }

            return_code = lzma_code(&strm_lzma, flushmode_lzma);

            out_size = strm_lzma.avail_out;
            return return_code;
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
    }

    return -1;
}

size_t compressor::data_left_in() {
    switch(comp_mode) {
    case C_ZLIB:
        return strm_zlib.avail_in;
        break;

    case C_LZMA:
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
        return strm_lzma.avail_out;
        break;
    }

    return -1;
};