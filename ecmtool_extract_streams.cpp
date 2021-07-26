/**
 * @file ecmtool_extract_streams.cpp
 * @author Daniel Carrasco (www.electrosoftcloud.com)
 * @brief This tool is intended to extract the compressed streams to debug the compression methods
 * @version 1.0
 * @date 2021-07-25
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "common.h"
#include "sector_tools.h"
#include <getopt.h>
#include <stdbool.h>
#include <string>
#include <stdexcept>
#include <algorithm>

// Configurations
#define BUFFER_SIZE 0x800000lu
#define DECOMPRESS_CHUNK_SIZE 0x150000llu

// Streams and sectors structs
#pragma pack(push, 1)
struct STREAM {
    uint8_t type : 1;
    uint8_t compression : 3;
    uint32_t end_sector;
    uint32_t out_end_position;
};

struct SECTOR {
    uint8_t mode : 4;
    uint32_t sector_count;
};

struct SEC_STR_SIZE {
    uint32_t count;
    uint32_t size;
};
#pragma pack(pop) 

// Declare the functions
void print_help();
static int8_t extract(const char* infilename);

int decompress_header (
    uint8_t* dest,
    uint32_t &destLen,
    uint8_t* source,
    uint32_t sourceLen
);
static void resetcounter(off_t total);
static void decode_progress(void);
static void setcounter_decode(off_t n);

// Some necessary variables
static off_t mycounter_decode  = (off_t)-1;
static off_t mycounter_total   = 0;


////////////////////////////////////////////////////////////////////////////////
//
// Returns nonzero on error
//
////////////////////////////////////////////////////////////////////////////////

static struct option long_options[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char** argv) {
    // options
    char* infilename  = NULL;

    // Decode
    bool decode = false;

    // temporal variables for options parsing
    uint64_t temp_argument = 0;
    const char *errstr;

    char ch;
    while ((ch = getopt_long(argc, argv, "i:", long_options, NULL)) != -1)
    {
        // check to see if a single character or long option came through
        switch (ch)
        {
            // short option '-i', long option '--input'
            case 'i':
                infilename = optarg;
                break;

            case '?':
                print_help();
                return 0;
                break;
        }
    }

    if (infilename == NULL) {
        printf("Error: input file is required.\n");
        print_help();
        return 1;
    }

    // The in file is an ECM file, will be decoded
    FILE* in  = NULL;
    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return 1;
    }
    else {
        if(
            (fgetc(in) == 'E') &&
            (fgetc(in) == 'C') &&
            (fgetc(in) == 'M') &&
            (fgetc(in) == 0x02)
        ) {
            printf("ECM2 file detected. I will continue.\n");
            fclose(in);
        }
        else {
            printf("The input file is not an ECM2 file...\n");
            fclose(in);
            return 1;
        }
        
    }

    extract(infilename);
}

static int8_t extract(const char* infilename) {
    // IN/OUT files definition
    FILE* in  = NULL;
    uint8_t return_code = 0;

    // Buffers initialization
    // Allocate input buffer space
    char* in_buffer = NULL;
    in_buffer = (char*) malloc(BUFFER_SIZE);
    if(!in_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    // Open input file
    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return 1;
    }
    setvbuf(in, in_buffer, _IOFBF, BUFFER_SIZE);
    // Getting the input size to set the progress
    fseeko(in, 0, SEEK_END);
    size_t in_total_size = ftello(in);
    fseeko(in, 5, SEEK_SET);
    // Reset the counters
    resetcounter(in_total_size);

    // Get the options used at file creation
    optimization_options options;
    fread(&options, sizeof(options), 1, in);

    // Streams TOC
    SEC_STR_SIZE streams_toc_count = {0, 0};
    fread(&streams_toc_count, sizeof(streams_toc_count), 1, in);
    STREAM *streams_toc = new STREAM[streams_toc_count.count];
    fread(streams_toc, sizeof(STREAM) * streams_toc_count.count, 1, in);

    // Sectors TOC
    SEC_STR_SIZE sectors_toc_count = {0, 0};
    fread(&sectors_toc_count, sizeof(sectors_toc_count), 1, in);
    SECTOR *sectors_toc = new SECTOR[sectors_toc_count.count];
    char* sectors_toc_c_buffer = (char*) malloc(sectors_toc_count.size);
    if(!sectors_toc_c_buffer) {
        printf("Out of memory\n");
        return_code = 1;
    }

    // Check all sectors (Must be added)
    uint32_t out_size = sectors_toc_count.count * sizeof(SECTOR);
    if (!return_code) {
        fread(sectors_toc_c_buffer, sectors_toc_count.size, 1, in);
        
        if (
            decompress_header(
                (uint8_t*)sectors_toc,
                out_size, 
                (uint8_t*)sectors_toc_c_buffer,
                sectors_toc_count.size
            )
        ) {
            printf("There was an error reading the header\n");
            return_code = 1;
        }
    }

    // Allocate compressor buffer
    uint8_t* decomp_buffer = (uint8_t*) malloc(BUFFER_SIZE);;
    if(!decomp_buffer) {
        printf("Out of memory\n");
        return_code = 1;
    }

    // Write the header to a file
    FILE * output_header = fopen("ecm2_headers.bin", "wb");
    uint8_t header[5];
    header[0] = 'E';
    header[1] = 'C';
    header[2] = 'M';
    header[3] = 0x02; // ECM version. For now 0 is the original version and 1 the new version
    header[4] = (uint8_t)0;
    fwrite(header, 5, 1, output_header);
    //
    fwrite(&options, sizeof(options), 1, output_header);
    //
    fwrite(&streams_toc_count, sizeof(streams_toc_count), 1, output_header);
    fwrite(streams_toc, sizeof(STREAM) * streams_toc_count.count, 1, output_header);
    //
    fwrite(&sectors_toc_count, sizeof(sectors_toc_count), 1, output_header);
    fwrite(sectors_toc_c_buffer, sectors_toc_count.size, 1, output_header);
    //
    fclose(output_header);

    // Output files
    FILE * out_stream_cmp = fopen("ecm2_cmp.bin", "wb");
    FILE * out_stream_uncmp = fopen("ecm2_uncmp.bin", "wb");

    //
    // Current sector type (run)
    //
    uint32_t current_sector = 1;

    // Initializing the Sector Tools object
    //sector_tools sTools = sector_tools();

    // Decompressor
    compressor *decompobj = NULL;

    // End Of Stream mark
    bool end_of_stream = false;

    //
    // Starting the decoding part to generate the output file
    //
    for (uint32_t streams_toc_actual = 0; streams_toc_actual < streams_toc_count.count; streams_toc_actual++) {
        if (return_code) { break; } // If there was an error, break the loop

        // If stream is compressed and there's no decomp object, create a buffer in memory
        // for the decompression, fill that buffet with the stream data and create the decomp 
        // object.
        if (streams_toc[streams_toc_actual].compression && !decompobj) {
            // Create a new decompressor object
            decompobj = new compressor((sector_tools_compression)streams_toc[streams_toc_actual].compression, false);
        }

        size_t stream_left = streams_toc[streams_toc_actual].out_end_position - ftello(in);
        size_t buffer_left = 0;
        // Read until stream is fully readed
        while (stream_left || buffer_left) {
            if (feof(in)){
                printf("Unexpected EOF detected.\n");
                printfileerror(in, infilename);
                return_code = 1;
                break;
            }

            // If not in end of stream and buffer is below 25%, read more data
            // To keep the buffer always ready
            if (buffer_left < stream_left && buffer_left < (BUFFER_SIZE * 0.25)) {
                // Move the left data to first bytes
                size_t position = BUFFER_SIZE - buffer_left;
                memmove(decomp_buffer, decomp_buffer + position, buffer_left);

                // Calculate how much data can be readed
                size_t to_read = BUFFER_SIZE - buffer_left;
                // If available space is bigger than data in stream, read only the stream data
                if (to_read > stream_left) {
                    to_read = stream_left;
                }
                // Fill the buffer with the stream data
                fread(decomp_buffer + buffer_left, to_read, 1, in);
                // Set again the input position to first byte in decomp_buffer and set the buffer size
                buffer_left += to_read;
                if (decompobj) {
                    // Set the input buffer position as "input" in decompressor object
                    decompobj -> set_input(decomp_buffer, buffer_left);
                }

                // Write the original stream
                fwrite(decomp_buffer, to_read, 1, out_stream_cmp);
            }

            switch (streams_toc[streams_toc_actual].compression) {
            // No compression
            case C_NONE:
                {
                    size_t to_copy = std::min(buffer_left, DECOMPRESS_CHUNK_SIZE);
                    fwrite(decomp_buffer, to_copy, 1, out_stream_uncmp);
                    buffer_left -= to_copy;
                    stream_left -= to_copy;
                    break;
                }
                
            // Zlib/LZMA/LZ4 compression
            case C_ZLIB:
            case C_LZMA:
            case C_LZ4:
                {
                    size_t decompress_buffer_left = 0;
                    uint8_t * output = (uint8_t *) malloc(DECOMPRESS_CHUNK_SIZE);
                    // Decompress the sector data
                    decompobj -> decompress(output, DECOMPRESS_CHUNK_SIZE, decompress_buffer_left, Z_SYNC_FLUSH);
                    size_t decompressed = DECOMPRESS_CHUNK_SIZE - decompobj->data_left_out();
                    fwrite(output, decompressed, 1, out_stream_uncmp);
                    free(output);

                    buffer_left = decompress_buffer_left;
                    stream_left = streams_toc[streams_toc_actual].out_end_position - ftell(in);
                }
            }

            // Set the current position in file
            setcounter_decode(ftello(in));
        }

        // Close decompression object if exists
        if (decompobj) {
            decompobj -> close();
            decompobj = NULL;
        }

        // Free the decomp buffer
        if (decomp_buffer) {
            free(decomp_buffer);
        }
    }

    // Flushing data and closing files
    fflush(out_stream_cmp);
    fflush(out_stream_uncmp);
    fclose(in);
    fclose(out_stream_cmp);
    fclose(out_stream_uncmp);
    // Freeing reserved memory
    free(in_buffer);
    delete[] sectors_toc;
    delete[] streams_toc;
    return return_code;
}

void print_help() {
    printf(
        "Usage:\n"
        "\n"
        "ecmtool_extract_streams -i/--input cdimagefile\n"
    );
}

static void decode_progress(void) {
    off_t d = (mycounter_decode  + 64) / 128;
    off_t t = (mycounter_total   + 64) / 128;
    if(!t) { t = 1; }
    fprintf(stderr,
        "Decode(%02u%%)\r",
        (unsigned)((((off_t)100) * d) / t)
    );
}

static void resetcounter(off_t total) {
    mycounter_decode  = (off_t)-1;
    mycounter_total   = total;
}

static void setcounter_decode(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_decode >> 20));
    mycounter_decode = n;
    if(p) { decode_progress(); }
}

int decompress_header (
    uint8_t* dest,
    uint32_t &destLen,
    uint8_t* source,
    uint32_t sourceLen
) {
    z_stream strm;
    int err;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    err = inflateInit(&strm);
    if (err != Z_OK) return err;

    strm.next_out = dest;
    strm.avail_out = destLen;
    strm.next_in = source;
    strm.avail_in = sourceLen;

    err = inflate(&strm, Z_NO_FLUSH);
    inflateEnd(&strm);

    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && strm.avail_out ? Z_DATA_ERROR :
           err;
}