#include <stdexcept>
#include "compressor.h"

compressor::compressor(sector_tools_compression mode, bool is_compression, int8_t comp_level) {
    comp_mode = mode;
    compression = is_compression;
    // Class initialzer
    if (mode == C_DATA_ZLIB) {
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        int ret;
        if (is_compression) {
            ret = deflateInit(&strm, comp_level);
        }
        else {
            ret = inflateInit(&strm);
        }
        
        if (ret != Z_OK) {
            //throw std::runtime_error("Error initializing the zlib compressor/decompressor.");
        }
    }
}

int8_t compressor::set_output(uint8_t* out, size_t &out_size){
    if (compression) {
        if (comp_mode == C_DATA_ZLIB) {
            strm.avail_out = out_size;
            strm.next_out = out;

            return 0;
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a decompression object to compress.");
        return -1;
    }
}

int8_t compressor::compress(size_t &out_size, uint8_t* in, size_t in_size, uint8_t flusmode){
    if (compression) {
        if (comp_mode == C_DATA_ZLIB) {
            strm.avail_in = in_size;
            strm.next_in = in;

            int8_t return_code = deflate(&strm, flusmode);

            out_size = strm.avail_out;
            return return_code;
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a decompression object to compress.");
        return -1;
    }
}

int8_t compressor::decompress(uint8_t* out, size_t out_size, uint8_t* in, size_t in_size, uint8_t flusmode){
    if (!compression) {
        if (comp_mode == C_DATA_ZLIB) {
            strm.avail_in = in_size;
            strm.next_in = in;

            strm.avail_out = out_size;
            strm.next_out = out;
            return inflate(&strm, flusmode);
        }

        return 0;
    }
    else {
        //throw std::runtime_error("Trying to use a compression object to decompress.");
        return -1;
    }
}

int8_t compressor::close(){
    if (compression) {
        (void)deflateEnd(&strm);
    }
    else {
        (void)inflateEnd(&strm);
    }
    return 0;
}