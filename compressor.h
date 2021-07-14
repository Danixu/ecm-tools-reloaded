#include <stdint.h>
#include "zlib.h"
#include "lzma.h"

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
        compressor(sector_tools_compression mode, bool is_compression, int8_t comp_level = -1);

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
        sector_tools_compression comp_mode;
        bool compression;
};