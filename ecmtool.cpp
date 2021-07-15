////////////////////////////////////////////////////////////////////////////////
//
#define TITLE "ecmtool - Encoder/decoder for Error Code Modeler format, with advanced features"
#define COPYR "Copyright (C) 2021 Daniel Carrasco"
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

#ifndef COMMON
#define COMMON
#endif
#include "common.h"
#include "banner.h"
#include "sector_tools.h"
#include <getopt.h>
#include <stdbool.h>
#include <string>
#include <stdexcept>
#include "zlib.h"

// Configurations
#define SECTORS_PER_BLOCK 100

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
static int8_t ecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite,
    sector_tools_compression data_compression,
    sector_tools_compression audio_compression,
    uint8_t compression_level,
    bool extreme_compression,
    bool seekable,
    uint8_t sectors_per_block
);
static int8_t unecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
);
int compress_header (
    uint8_t* dest,
    uint32_t &destLen,
    uint8_t* source,
    uint32_t sourceLen,
    int level
);
int decompress_header (
    uint8_t* dest,
    uint32_t &destLen,
    uint8_t* source,
    uint32_t sourceLen
);
static void resetcounter(off_t total);
static void encode_progress(void);
static void decode_progress(void);
static void setcounter_analyze(off_t n);
static void setcounter_encode(off_t n);
static void setcounter_decode(off_t n);

// Some necessary variables
static off_t mycounter_analyze = (off_t)-1;
static off_t mycounter_encode  = (off_t)-1;
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
    {"acompression", required_argument, NULL, 'a'},
    {"dcompression", required_argument, NULL, 'd'},
    {"clevel", required_argument, NULL, 'c'},
    {"extreme-compression", no_argument, NULL, 'e'},
    {"seekable", no_argument, NULL, 's'},
    {"sectors_per_block", required_argument, NULL, 'p'},
    {"force", required_argument, NULL, 'f'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char** argv) {
    // options
    char* infilename  = NULL;
    char* outfilename = NULL;
    char* tempfilename = NULL;
    sector_tools_compression audio_compression = C_NONE;
    sector_tools_compression data_compression = C_NONE;
    uint8_t compression_level = 5;
    bool extreme_compression = false;
    bool force_rewrite = false;
    // ZLIB will be seekable
    bool seekable = false;
    uint8_t sectors_per_block = SECTORS_PER_BLOCK;

    // Decode
    bool decode = false;

    // temporal variables for options parsing
    uint64_t temp_argument = 0;
    const char *errstr;

    char ch;
    while ((ch = getopt_long(argc, argv, "i:o:a:d:c:esp:f", long_options, NULL)) != -1)
    {
        // check to see if a single character or long option came through
        switch (ch)
        {
            // short option '-i', long option '--input'
            case 'i':
                infilename = optarg;
                break;

            // short option '-o', long option "--output"
            case 'o':
                outfilename = optarg;
                break;

            // short option '-a', long option "--acompression"
            // Audio compression option
            case 'a':
                if (strcmp("zlib", optarg) == 0) {
                    audio_compression = C_ZLIB;
                }
                else if (strcmp("lzma", optarg) == 0) {
                    audio_compression = C_LZMA;
                }
                else {
                    printf("Error: Unknown data compression mode: %s\n\n", optarg);
                    print_help();
                    return 1;
                }
                break;

            // short option '-d', long option '--dcompression'
            // Data compression option
            case 'd':
                if (strcmp("zlib", optarg) == 0) {
                    data_compression = C_ZLIB;
                }
                else if (strcmp("lzma", optarg) == 0) {
                    data_compression = C_LZMA;
                }
                else {
                    printf("Error: Unknown data compression mode: %s\n\n", optarg);
                    print_help();
                    return 1;
                }
                break;

            // short option '-c', long option "--clevel"
            case 'c':
                try {
                    std::string optarg_s(optarg);
                    temp_argument = std::stoi(optarg_s);

                    if (temp_argument > 9 || temp_argument < 0) {
                        printf("Error: the provided compression level option is not correct.\n\n");
                        print_help();
                        return 1;
                    }
                    else {
                        compression_level = (uint8_t)temp_argument;
                    }
                } catch (std::exception const &e) {
                    printf("Error: the provided compression level option is not correct.\n\n");
                    print_help();
                    return 1;
                }

                break;

            // short option '-e', long option "--extreme-compresison" (only LZMA)
            case 'e':
                extreme_compression = true;
                break;

             // short option '-s', long option "--seekable"
            case 's':
                seekable = true;
                break;

            // short option '-p', long option "--sectors_per_block"
            case 'p':
                try {
                    std::string optarg_s(optarg);
                    temp_argument = std::stoi(optarg_s);

                    if (!temp_argument || temp_argument > 255 || temp_argument < 0) {
                        printf("Error: the provided sectors per block number is not correct.\n\n");
                        print_help();
                        return 1;
                    }
                    else {
                        sectors_per_block = (uint8_t)temp_argument;
                    }
                } catch (std::exception const &e) {
                    printf("Error: the provided sectors per block number is not correct.\n\n");
                    print_help();
                    return 1;
                }
                break;

            // short option '-f', long option "--force"
            case 'f':
                force_rewrite = true;
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
            (fgetc(in) == 'M')
        ) {
            printf("An ECM file was detected... will be decoded\n");
            decode = true;

            if (fgetc(in) != 0x02) {
                // The file is in the old format, so compatibility will be enabled
                fclose(in);

                printf("The imput file is not compatible with this program version. The program will exit.\n");
                return 1;
            }
        }
        else {
            printf("A BIN file was detected... will be encoded\n");
        }
        fclose(in);
    }

    // If not output filename was provided, we will generate one
    // based in the original filename.
    if (outfilename == NULL) {
        tempfilename = (char*) malloc(strlen(infilename) + 7);
        if(!tempfilename) {
            printf("Out of memory\n");
            return 1;
        }

        strcpy(tempfilename, infilename);

        if(decode) {
            //
            // Remove ".ecm" from the input filename
            //
            size_t l = strlen(tempfilename);
            if(
                (l > 4) &&
                tempfilename[l - 5] == '.' &&
                tolower(tempfilename[l - 4]) == 'e' &&
                tolower(tempfilename[l - 3]) == 'c' &&
                tolower(tempfilename[l - 2]) == 'm' &&
                tolower(tempfilename[l - 1]) == '2'
            ) {
                tempfilename[l - 5] = 0;
            } else {
                //
                // If that fails, append ".unecm" to the input filename
                //
                strcat(tempfilename, ".unecm2");
            }
        }
        else {
            //
            // Append ".ecm" to the input filename
            //
            strcat(tempfilename, ".ecm2");
        }
        outfilename = tempfilename;
    }

    if (decode) {
        unecmify(infilename, outfilename, force_rewrite);
    }
    else {
        ecmify(
            infilename,
            outfilename,
            force_rewrite,
            data_compression,
            audio_compression,
            compression_level,
            extreme_compression,
            seekable,
            sectors_per_block
        );
    }
}


static int8_t ecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite,
    sector_tools_compression data_compression,
    sector_tools_compression audio_compression,
    uint8_t compression_level,
    bool extreme_compression,
    bool seekable,
    uint8_t sectors_per_block
) {
    // IN/OUT files definition
    FILE* in  = NULL;
    FILE* out = NULL;
    uint8_t return_code = 0;

    // Optimization options
    optimization_options optimizations = (
        OO_REMOVE_SYNC |
        OO_REMOVE_ADDR |
        OO_REMOVE_MODE |
        OO_REMOVE_BLANKS |
        OO_REMOVE_REDUNDANT_FLAG |
        OO_REMOVE_ECC |
        OO_REMOVE_EDC |
        OO_REMOVE_GAP
    );

    if (!force_rewrite) {
        out = fopen(outfilename, "rb");
        if (out) {
            printf("Error: %s exists; refusing to overwrite\n", outfilename);
            fclose(out);
            return 1;
        }
    }

    // Buffers size
    size_t buffer_size = ((size_t)(-1)) - 4095;
    if((unsigned long)buffer_size > 0x40000lu) {
        buffer_size = (size_t)0x40000lu;
    }

    // Buffers initialization
    // Allocate input buffer space
    char* in_buffer = NULL;
    in_buffer = (char*) malloc(buffer_size);
    if(!in_buffer) {
        printf("Out of memory\n");
        return 1;
    }
    // Allocate output buffer space
    char* out_buffer = NULL;
    out_buffer = (char*) malloc(buffer_size);
    if(!out_buffer) {
        printf("Out of memory\n");
        return 1;
    }
    // Allocate compressor buffer if required
    uint8_t* comp_buffer = NULL;
    if (data_compression || audio_compression) {
        comp_buffer = (uint8_t*) malloc(buffer_size);
        if(!comp_buffer) {
            printf("Out of memory\n");
            return 1;
        }
    }

    // Open input file
    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return 1;
    }
    setvbuf(in, in_buffer, _IOFBF, buffer_size);

    // We will check if image is a CD-ROM
    // CD-ROMS sectors are 2352 bytes size and are always filled, so image size must be multiple
    fseeko(in, 0, SEEK_END);
    size_t in_total_size = ftello(in);
    if (in_total_size % 2352) {
        printf("ERROR: The input file doesn't appear to be a CD-ROM image\n");
        printf("       This program only allows to process CD-ROM images\n");

        fclose(in);
        return 1;
    }
    fseeko(in, 0, SEEK_SET);
    // Reset the counters
    resetcounter(in_total_size);

    // Open output file
    out = fopen(outfilename, "wb");
    if (!out) {
        printfileerror(out, infilename);
        return 1;
    }
    setvbuf(out, out_buffer, _IOFBF, buffer_size);

    // CRC calculation to check the decoded stream
    uint32_t input_edc = 0;

    // Sectors TOC
    SECTOR *sectors_toc = new SECTOR[0xFFFFF];
    SEC_STR_SIZE sectors_toc_count = {0, 0};

    // Streams TOC
    STREAM *streams_toc = new STREAM[0xFFFFF];
    SEC_STR_SIZE streams_toc_count = {0, 0};
    sector_tools_stream_types curstreamtype = STST_UNKNOWN; // not a valid type

    //
    // Current sector type (run)
    //
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    uint32_t           curtype_count = 0;
    uint32_t           current_sector = 0;

    // Sector Tools object
    sector_tools sTools = sector_tools();

    // Compressor
    compressor *compobj = NULL;

    // Sector buffer
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    //
    // Starting the analyzing part to generate the TOC header
    //
    while (!feof(in)) {
        // Read the next sector
        if (fread(in_sector, 2352, 1, in)) {
            // Add a sector to the count
            current_sector++;
            // Update the input file position
            setcounter_analyze(ftello(in));

            sector_tools_types detected_type = sTools.detect(in_sector);

            if (detected_type == curtype) {
                // Same type as last sector
                curtype_count++;
            }
            else {
                // Checking if the stream type is different than the last one
                sector_tools_stream_types stream_type = sTools.detect_stream(detected_type);

                if (curstreamtype == STST_UNKNOWN) {
                    // First stream position. We need the end of stream, so we will just save the type
                    curstreamtype = stream_type;
                }
                else if (stream_type != curstreamtype) {
                    // nTH stream, saving the position as "END" of the last stream
                    sector_tools_compression compression;
                    if (curstreamtype == STST_AUDIO) {
                        compression = audio_compression;
                    }
                    else {
                        compression = data_compression;
                    }
                    streams_toc[streams_toc_count.count] = {
                        (bool)(curstreamtype - 1),
                        compression,
                        current_sector
                    };
                    streams_toc_count.count++;
                    curstreamtype = stream_type;
                }

                if(curtype_count > 0) {
                    // Generate the sector mode data
                    sectors_toc[sectors_toc_count.count] = {
                        curtype,
                        curtype_count
                    };

                    sectors_toc_count.count++;
                }

                curtype = detected_type;
                curtype_count = 1;
            }
        }
        else if(ferror(in)) {
            // Something strange happens
            printf("\n\nThere was an error analyzing the input file...\n");
            printfileerror(in, infilename);
            return_code = 1;
        }
    }

    if(!return_code) {
        // Set the last stream end position
        sector_tools_compression compression;
        if (curstreamtype == STST_AUDIO) {
            compression = audio_compression;
        }
        else {
            compression = data_compression;
        }
        streams_toc[streams_toc_count.count] = {
            (bool)(curstreamtype - 1),
            compression,
            current_sector,
            0
        };
        streams_toc_count.count++;

        // Set the last sectors type and count
        sectors_toc[sectors_toc_count.count] = {
            curtype,
            curtype_count
        };
        sectors_toc_count.count++;

        // Compress the sectors header
        uint32_t sectors_toc_size = sectors_toc_count.count * sizeof(struct SECTOR);
        char* sectors_toc_c_buffer = (char*) malloc(sectors_toc_size);
        if(!sectors_toc_c_buffer) {
            printf("Out of memory\n");
            return_code = 1;
        }

        if(!return_code) {
            compress_header((uint8_t*)sectors_toc_c_buffer, sectors_toc_size, (uint8_t*)sectors_toc, sectors_toc_size, 9);

            // Set the size of header
            streams_toc_count.size = streams_toc_count.count * sizeof(struct STREAM);
            sectors_toc_count.size = sectors_toc_size;

            // Once the file is analyzed and we know the TOC, we will process al the data
            //
            //printf("Writting header and TOC data to output buffer\n");
            // Add the Header to the output file
            uint8_t header[5];
            header[0] = 'E';
            header[1] = 'C';
            header[2] = 'M';
            header[3] = 0x02; // ECM version. For now 0 is the original version and 1 the new version
            header[4] = seekable ? sectors_per_block : (uint8_t)0;
            fwrite(header, 5, 1, out);
            fwrite(&optimizations, sizeof(optimizations), 1, out);

            // Write the Streams TOC to the output file
            fwrite(&streams_toc_count, sizeof(streams_toc_count), 1, out);
            fwrite(streams_toc, streams_toc_count.count * sizeof(struct STREAM), 1, out);
            // Write the Sectors TOC to the output file
            fwrite(&sectors_toc_count, sizeof(sectors_toc_count), 1, out);
            fwrite(sectors_toc_c_buffer, sectors_toc_size, 1, out);

            // Reset the input file position
            fseeko(in, 0, SEEK_SET);
            // Reset the current sector count
            current_sector = 1;
        }
    }

    //
    // Starting the processing part
    //
    uint32_t streams_toc_actual = 0;
    // Loop through every sector type in toc
    for (uint32_t sectors_toc_actual = 0; sectors_toc_actual < sectors_toc_count.count; sectors_toc_actual++) {
        if (return_code) { break; } // If there was an error, break the loop

        // If the next sector correspond to the next stream, advance to it
        if (current_sector == streams_toc[streams_toc_actual].end_sector) {
            if (compobj) {
                size_t compress_buffer_left = 0;
                compobj -> compress(compress_buffer_left, out_sector, 0, Z_FINISH);
                fwrite(comp_buffer, buffer_size - compress_buffer_left, 1, out);
                compobj -> close();
                compobj = NULL;
            }

            streams_toc[streams_toc_actual].out_end_position = ftello(out);
            streams_toc_actual++;
        }

        if (streams_toc[streams_toc_actual].compression && !compobj) {
            uint8_t compression_option = compression_level;
            if (
                (sector_tools_compression)streams_toc[streams_toc_actual].compression == C_LZMA &&
                extreme_compression    
            ) {
                compression_option |= LZMA_PRESET_EXTREME;
            }
            compobj = new compressor(
                (sector_tools_compression)streams_toc[streams_toc_actual].compression,
                true,
                compression_option
            );
            compobj -> set_output(comp_buffer, buffer_size);
        }

        // Select the compression depending of the stream type and the user options
        sector_tools_compression current_compression;
        if ((streams_toc[streams_toc_actual].type + 1) == STST_AUDIO) {
            current_compression = audio_compression;
        }
        else {
            current_compression = data_compression;
        }

        // Process the sectors count indicated in the toc
        for (curtype_count = 0; curtype_count < sectors_toc[sectors_toc_actual].sector_count; curtype_count++) {
            if (feof(in)){
                printf("Unexpected EOF detected.\n");
                printfileerror(in, infilename);
                return_code = 1;
                break;
            }

            fread(in_sector, 2352, 1, in);
            // Compute the crc of the readed data 
            input_edc = sTools.edc_compute(
                input_edc,
                in_sector,
                2352
            );

            // We will clean the sector to keep only the data that we want
            uint16_t output_size = 0;
            sTools.clean_sector(
                out_sector,
                in_sector,
                (sector_tools_types)sectors_toc[sectors_toc_actual].mode,
                output_size,
                optimizations
            );

            // Compress the sector using the selected compression (or none)
            switch (current_compression) {
            // No compression
            case C_NONE:
                fwrite(out_sector, output_size, 1, out);
                break;

            // Zlib compression
            case C_ZLIB:
            case C_LZMA:
                size_t compress_buffer_left = 0;
                // Current sector is the last stream sector
                if ((current_sector+1) == streams_toc[streams_toc_actual].end_sector) {
                    compobj -> compress(compress_buffer_left, out_sector, output_size, Z_FINISH);
                }
                else if (seekable && (sectors_per_block == 1 || !((current_sector + 1) % sectors_per_block))) {
                    // A new compressor block is required
                    compobj -> compress(compress_buffer_left, out_sector, output_size, Z_FULL_FLUSH);
                }
                else {
                    compobj -> compress(compress_buffer_left, out_sector, output_size, Z_NO_FLUSH);
                }

                // If buffer is above 75% or is the last sector, write the data to the output and reset the state
                if (compress_buffer_left < (buffer_size * 0.25) || (current_sector+1) == streams_toc[streams_toc_actual].end_sector) {
                    fwrite(comp_buffer, buffer_size - compress_buffer_left, 1, out);
                    compobj -> set_output(comp_buffer, buffer_size);
                }
                break;
            }

            setcounter_encode(ftello(in));
            // If we are not in end of file, sum a sector
            if (ftello(in) != in_total_size) {
                current_sector++;
            }
       }  
    }

    // Close the compression object
    if (compobj) {
        compobj -> close();
        compobj = NULL;
    }

    streams_toc[streams_toc_actual].out_end_position = ftello(out);

    // Add the CRC to the output file
    uint8_t crc[4];
    sTools.put32lsb(crc, input_edc);
    fwrite(crc, 4, 1, out);

    // Rewrite the streams header to store the new data (end position in output file)
    fseeko(out, 5 + sizeof(streams_toc_count) + sizeof(optimizations), SEEK_SET);
    fwrite(streams_toc, streams_toc_count.count * sizeof(struct STREAM), 1, out);

    // End Of TOC reached, so file should have be processed completly
    if (ftello(in) != in_total_size) {
        printf("\n\nThere was an error processing the input file...\n");
        printfileerror(in, infilename);
        return_code = 1;
    }

    fclose(in);
    fflush(out);
    fclose(out);
    free(in_buffer);
    free(out_buffer);
    delete[] sectors_toc;
    delete[] streams_toc;

    if (!return_code) {
        printf("\n\nFinished!\n");
    }
    return return_code;
}


static int8_t unecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
) {
    // IN/OUT files definition
    FILE* in  = NULL;
    FILE* out = NULL;
    uint8_t return_code = 0;

    if (!force_rewrite) {
        out = fopen(outfilename, "rb");
        if (out) {
            printf("Error: %s exists; refusing to overwrite\n", outfilename);
            fclose(out);
            return 1;
        }
    }

    // Buffers size
    size_t buffer_size = ((size_t)(-1)) - 4095;
    if((unsigned long)buffer_size > 0x40000lu) {
        buffer_size = (size_t)0x40000lu;
    }

    // Buffers initialization
    // Allocate input buffer space
    char* in_buffer = NULL;
    in_buffer = (char*) malloc(buffer_size);
    if(!in_buffer) {
        printf("Out of memory\n");
        return 1;
    }
    // Allocate output buffer space
    char* out_buffer = NULL;
    out_buffer = (char*) malloc(buffer_size);
    if(!out_buffer) {
        printf("Out of memory\n");
        return 1;
    }
    // Allocate compressor buffer if required
    uint8_t* decomp_buffer = NULL;

    // Open input file
    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return 1;
    }
    setvbuf(in, in_buffer, _IOFBF, buffer_size);
    // Getting the input size to set the progress
    fseeko(in, 0, SEEK_END);
    size_t in_total_size = ftello(in);
    fseeko(in, 5, SEEK_SET);
    // Reset the counters
    resetcounter(in_total_size);
    // Open output file
    out = fopen(outfilename, "wb");
    if (!in) {
        printfileerror(out, outfilename);
        return 1;
    }
    setvbuf(out, out_buffer, _IOFBF, buffer_size);

    // Get the options used at file creation
    optimization_options options;
    fread(&options, sizeof(options), 1, in);

    // CRC calculator
    uint32_t original_edc = 0;
    uint32_t output_edc = 0;

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

    if (!return_code) {
        fread(sectors_toc_c_buffer, sectors_toc_count.size, 1, in);

        uint32_t out_size = sectors_toc_count.count * sizeof(SECTOR);
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

    //
    // Current sector type (run)
    //
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    uint32_t           current_sector = 1;

    // Initializing the Sector Tools object
    sector_tools sTools = sector_tools();

    // Decompressor
    compressor *decompobj = NULL;

    // Sector buffer
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    // End Of Stream mark
    bool end_of_stream = false;

    //
    // Starting the decoding part to generate the output file
    //
    uint32_t streams_toc_actual = 0;
    for (uint32_t sectors_toc_actual = 0; sectors_toc_actual < sectors_toc_count.count; sectors_toc_actual++) {
        if (return_code) { break; } // If there was an error, break the loop

        // If the next sector correspond to the next stream, advance to it
        if (current_sector == streams_toc[streams_toc_actual].end_sector) {
            // New stream, new state. Reset the stream state
            end_of_stream = false;
            
            // No flush required as the last sector should have been read
            if (decompobj) {
                decompobj -> close();
                decompobj = NULL;
            }

            // Free the decomp buffer to avoid memory leak
            if (decomp_buffer) {
                free(decomp_buffer);
            }
            streams_toc_actual++;
        }

        // If stream is compressed and there's no decomp object, create a buffer in memory
        // for the decompression, fill that buffet with the stream data and create the decomp 
        // object.
        if (streams_toc[streams_toc_actual].compression && !decompobj) {
            // Create the decompression buffer
            decomp_buffer = (uint8_t*) malloc(buffer_size);
            if(!decomp_buffer) {
                printf("Out of memory\n");
                return_code = 1;
                break;
            }
            // Check if stream size is smaller than the buffer size and use the smaller size as "to_read"
            size_t to_read = buffer_size;
            size_t stream_size = streams_toc[streams_toc_actual].out_end_position - ftello(in);
            if (to_read > stream_size) {
                to_read = stream_size;
                end_of_stream = true;
            }
            // Read the data into the buffer
            fread(decomp_buffer, to_read, 1, in);
            // Create a new decompressor object
            decompobj = new compressor((sector_tools_compression)streams_toc[streams_toc_actual].compression, false);
            // Set the input buffer position as "input" in decompressor object
            decompobj -> set_input(decomp_buffer, to_read);
        }

        // Process the sectors count indicated in the toc
        for (uint32_t curtype_count = 0; curtype_count < sectors_toc[sectors_toc_actual].sector_count; curtype_count++) {
            if (feof(in)){
                printf("Unexpected EOF detected.\n");
                printfileerror(in, infilename);
                return_code = 1;
                break;
            }

            uint16_t bytes_to_read = 0;
            // Getting the sector size prior to read, to read the real sector size and avoid to fseek every time
            sTools.encoded_sector_size(
                (sector_tools_types)sectors_toc[sectors_toc_actual].mode,
                bytes_to_read,
                options
            );

            size_t decompress_buffer_left = 0;
            switch (streams_toc[streams_toc_actual].compression) {
            // No compression
            case C_NONE:
                fread(in_sector, bytes_to_read, 1, in);
                break;

            // Zlib compression
            case C_ZLIB:
            case C_LZMA:
                // Decompress the sector data
                decompobj -> decompress(in_sector, bytes_to_read, decompress_buffer_left, Z_SYNC_FLUSH);

                // If not in end of stream and buffer is below 25%, read more data
                // To keep the buffer always ready
                if (!end_of_stream && decompress_buffer_left < (buffer_size * 0.25)) {
                    // Move the left data to first bytes
                    size_t position = buffer_size - decompress_buffer_left;
                    memmove(decomp_buffer, decomp_buffer + position, decompress_buffer_left);

                    // Calculate how much data can be readed
                    size_t to_read = buffer_size - decompress_buffer_left;
                    // If available space is bigger than data in stream, read only the stream data
                    size_t stream_size = streams_toc[streams_toc_actual].out_end_position - ftello(in);
                    if (to_read > stream_size) {
                        to_read = stream_size;  
                        // If left data is less than buffer space, then end_of_stream is reached
                        end_of_stream = true;
                    }
                    // Fill the buffer with the stream data
                    fread(decomp_buffer + decompress_buffer_left, to_read, 1, in);
                    // Set again the input position to first byte in decomp_buffer and set the buffer size
                    size_t input_size = decompress_buffer_left + to_read;
                    decompobj -> set_input(decomp_buffer, input_size);
                }

                break;
            }

            // Regenerating the sector data
            uint16_t bytes_readed = 0;
            sTools.regenerate_sector(
                out_sector,
                in_sector,
                (sector_tools_types)sectors_toc[sectors_toc_actual].mode,
                current_sector - 1 + 0x96, // 0x96 is the first sector "time", equivalent to 00:02:00
                bytes_readed,
                options
            );


            // Writting the sector to output file
            fwrite(out_sector, 2352, 1, out);
            // Compute the crc of the written data 
            output_edc = sTools.edc_compute(
                output_edc,
                out_sector,
                2352
            );

            // Set the current position in file
            setcounter_decode(ftello(in));
            current_sector++;
        }
    }

    // There is no more data in header. Next 4 bytes might be the CRC
    // Reading it...
    uint8_t buffer_edc[4];
    fread(buffer_edc, 4, 1, in);
    original_edc = sTools.get32lsb(buffer_edc);

    // Flushing data and closing files
    fflush(out);
    fclose(in);
    fclose(out);
    // Freeing reserved memory
    free(in_buffer);
    free(out_buffer);
    delete[] sectors_toc;
    delete[] streams_toc;
    if (original_edc == output_edc) {
        printf("\n\nFinished!\n");
    }
    else {
        printf("\n\nWrong CRC!... Maybe the input file is damaged.\n");
    }
    return return_code;
}

void print_help() {
    banner();
    printf(
        "Usage:\n"
        "\n"
        "To encode:\n"
        "    ecmtool -i/--input cdimagefile\n"
        "    ecmtool -i/--input cdimagefile -o/--output ecmfile\n"
        "\n"
        "To decode:\n"
        "    ecmtool -i/--input ecmfile\n"
        "    ecmtool -i/--input ecmfile -o/--output cdimagefile\n"
        "\n"
        "Optional options:\n"
        "    -a/--acompression <zlib/lzma>\n"
        "           Enable audio compression\n"
        "    -d/--dcompression <zlib/lzma>\n"
        "           Enable data compression\n"
        "    -c/--clevel <0-9>\n"
        "           Compression level between 0 and 9\n"
        "    -e/--extreme-compression\n"
        "           Enables extreme compression mode for LZMA (very slow)\n"
        "    -s/--seekable\n"
        "           Create a seekable file. Reduce the compression ratio but\n"
        "           but allow to seek into the stream.\n"
        "    -p/--sectors_per_block <sectors>\n"
        "           Add a end of block mark every X sectors in a seekable file. Max 255.\n"
        "    -f/--force\n"
        "           Force to ovewrite the output file\n"
    );
}


static void resetcounter(off_t total) {
    mycounter_analyze = (off_t)-1;
    mycounter_encode  = (off_t)-1;
    mycounter_decode  = (off_t)-1;
    mycounter_total   = total;
}


static void encode_progress(void) {
    off_t a = (mycounter_analyze + 64) / 128;
    off_t e = (mycounter_encode  + 64) / 128;
    off_t t = (mycounter_total   + 64) / 128;
    if(!t) { t = 1; }
    fprintf(stderr,
        "Analyze(%02u%%) Encode(%02u%%)\r",
        (unsigned)((((off_t)100) * a) / t),
        (unsigned)((((off_t)100) * e) / t)
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


static void setcounter_analyze(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_analyze >> 20));
    mycounter_analyze = n;
    if(p) { encode_progress(); }
}


static void setcounter_encode(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_encode >> 20));
    mycounter_encode = n;
    if(p) { encode_progress(); }
}


static void setcounter_decode(off_t n) {
    int8_t p = ((n >> 20) != (mycounter_decode >> 20));
    mycounter_decode = n;
    if(p) { decode_progress(); }
}

int compress_header (
    uint8_t* dest,
    uint32_t &destLen,
    uint8_t* source,
    uint32_t sourceLen,
    int level
) {
    z_stream strm;
    int err;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    err = deflateInit(&strm, 9);
    if (err != Z_OK) return err;

    strm.next_out = dest;
    strm.avail_out = destLen;
    strm.next_in = source;
    strm.avail_in = sourceLen;
    
    err = deflateInit(&strm, level);
    if (err != Z_OK) return err;

    err = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);

    destLen = strm.total_out;

    return err == Z_STREAM_END ? Z_OK : err;
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