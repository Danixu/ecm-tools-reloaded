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
#include "getopt.h"
#include "stdbool.h"

// Declare the functions
void print_help();
static int8_t ecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
);
static int8_t unecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
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
    {"compatible", no_argument, NULL, 'd'},
    {"compression", required_argument, NULL, 'c'},
    {"force", required_argument, NULL, 'f'},
    {NULL, 0, NULL, 0}
};

int main(int argc, char** argv) {
    // options
    char* infilename  = NULL;
    char* outfilename = NULL;
    char* tempfilename = NULL;
    int compression = 0;
    bool compatible = false;
    bool force_rewrite = false;

    // Decode
    bool decode = false;

    char ch;
    while ((ch = getopt_long(argc, argv, "i:o:dc:f", long_options, NULL)) != -1)
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
            // short option '-d', long option "--compatible"
            case 'd':
                compatible = true;
                break;
            // short option '-c', long option "--compression"
            case 'c':
                compression = true;
                break;
            // short option '-c', long option "--compression"
            case 'f':
                force_rewrite = true;
                break;
            case '?':
                print_help();
                return 1;
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

            if (fgetc(in) == 0x00) {
                // The file is in the old format, so compatibility will be enabled
                compatible = true;
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
                tempfilename[l - 4] == '.' &&
                tolower(tempfilename[l - 3]) == 'e' &&
                tolower(tempfilename[l - 2]) == 'c' &&
                tolower(tempfilename[l - 1]) == 'm'
            ) {
                tempfilename[l - 4] = 0;
            } else {
                //
                // If that fails, append ".unecm" to the input filename
                //
                strcat(tempfilename, ".unecm");
            }
        }
        else {
            //
            // Append ".ecm" to the input filename
            //
            strcat(tempfilename, ".ecm");
        }
        outfilename = tempfilename;
    }

    if (decode) {
        unecmify(infilename, outfilename, force_rewrite);
    }
    else {
        ecmify(infilename, outfilename, force_rewrite);
    }
}


static int8_t ecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
) {
    FILE* in  = NULL;
    FILE* out = NULL;
    uint8_t return_code = 0;

    // CRC calculator
    uint32_t input_edc = 0;

    // Input buffer
    uint8_t* in_queue = NULL;
    size_t in_queue_current_ofs = 0;
    size_t in_queue_bytes_available = 0;

    // Output buffer
    uint8_t* out_queue = NULL;
    size_t out_queue_current_ofs = 0;

    // Sectors TOC buffer
    uint8_t* sectors_toc_buffer = NULL;
    size_t sectors_toc_buffer_current_ofs = 0;

    // Streams TOC buffer
    uint8_t* streams_toc_buffer = NULL;
    size_t streams_toc_buffer_current_ofs = 0;
    sector_tools_stream_types curstreamtype = STST_UNKNOWN; // not a valid type

    //
    // Current sector type (run)
    //
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    uint32_t           curtype_count = 0;
    uint32_t           curtype_total = 0;

    // Buffers size
    size_t queue_size = ((size_t)(-1)) - 4095;
    if((unsigned long)queue_size > 0x40000lu) {
        queue_size = (size_t)0x40000lu;
    }

    if (!force_rewrite) {
        out = fopen(outfilename, "rb");
        if (out) {
            printf("Error: %s exists; refusing to overwrite\n", outfilename);
            fclose(out);
            return 1;
        }
    }

    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return 1;
    }

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

    out = fopen(outfilename, "wb");
    if (!out) {
        printfileerror(out, infilename);
        return 1;
    }

    // Allocate input buffer space
    in_queue = (uint8_t*) malloc(queue_size);
    if(!in_queue) {
        printf("Out of memory\n");
        return 1;
    }

    // Allocate sectors toc buffer space. A few kbytes is enought for most of the situations, but 
    // is not too much memory and is better to be sure
    sectors_toc_buffer = (uint8_t*) malloc(0x80000); // A 512Kb buffer to store the sectors TOC
    if(!sectors_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    // Allocate stream toc buffer space. A few kbytes is enought for most of the situations, but 
    // is not too much memory and is better to be sure
    streams_toc_buffer = (uint8_t*) malloc(0x40000); // A 256Kb buffer to store the stream TOC
    if(!streams_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    sector_tools sTools = sector_tools();

    //
    // Starting the analyzing part to generate the TOC header
    //
    while (!return_code) {
        sector_tools_types detected_type;

        // Refill queue if necessary
        if(
            !feof(in) &&
            (in_queue_bytes_available < 2352)
        ) {
            // We need to read more data
            size_t willread = queue_size - in_queue_bytes_available;
            
            // If the queue offset 
            if(in_queue_bytes_available > 0 && in_queue_current_ofs > 0) {
                memmove(in_queue, in_queue + in_queue_current_ofs, in_queue_bytes_available);
            }

            in_queue_current_ofs = 0;
            
            if(willread) {
                size_t readed = fread(in_queue + in_queue_bytes_available, 1, willread, in);
                
                if (ferror(in)) {
                    // Something happen while reading the input file
                    printfileerror(in, infilename);

                    // Exit with error code
                    return_code = 1;
                }

                setcounter_analyze(ftello(in));

                in_queue_bytes_available += readed;
            }
        }

        if (in_queue_bytes_available == 0 && feof(in)) {
            // If there are no bytes in queue and EOF was reached, break the loop
            detected_type = STT_UNKNOWN;
        }
        else {
            detected_type = sTools.detect(in_queue + in_queue_current_ofs);
        }

        in_queue_current_ofs += 2352;
        in_queue_bytes_available -= 2352;

        if(
            (detected_type == curtype) &&
            (curtype_count <= 0x7FFFFFFF) // avoid overflow
        ) {
            // Same type as last sector
            curtype_count++;
        }
        else {
            if(curtype_count > 0) {
                // Generate the sector mode data
                uint8_t generated_bytes;
                sTools.write_type_count(
                    sectors_toc_buffer + sectors_toc_buffer_current_ofs,
                    curtype,
                    curtype_count,
                    generated_bytes
                );
                sectors_toc_buffer_current_ofs += generated_bytes;

                // Checking if the stream type is different than the last one
                sector_tools_stream_types stream_type = sTools.detect_stream(curtype);
                if (stream_type != curstreamtype) {
                    uint8_t generated_bytes;
                    uint32_t current_in = ftello(in) - in_queue_bytes_available;
                    sTools.write_type_count(
                        streams_toc_buffer + streams_toc_buffer_current_ofs,
                        stream_type,
                        current_in,
                        generated_bytes
                    );
                    streams_toc_buffer_current_ofs += generated_bytes;
                    curstreamtype = stream_type;
                }
            }

            curtype = detected_type;
            curtype_count = 1;
        }

        // if curtype is negative at this point, then the EOF is reached
        if(curtype == STT_UNKNOWN) {
            // Write the End of Data to Sectors TOC buffer
            uint8_t generated_bytes;
            sTools.write_type_count(
                sectors_toc_buffer + sectors_toc_buffer_current_ofs,
                STT_UNKNOWN,
                0,
                generated_bytes
            );
            sectors_toc_buffer_current_ofs++;

            // Write the End of Data to Streams TOC buffer
            sTools.write_type_count(
                streams_toc_buffer + streams_toc_buffer_current_ofs,
                STST_UNKNOWN,
                0,
                generated_bytes
            );
            streams_toc_buffer_current_ofs += generated_bytes;
            break;
        }
    }

    // Once the file is analyzed and we know the TOC, we will process al the data
    //
    // Allocate output space for buffer
    out_queue = (uint8_t*) malloc(queue_size);
    if(!out_queue) {
        printf("Out of memory\n");
        return 1;
    }

    //printf("Writting header and TOC data to output buffer\n");
    // Add the Header to the output buffer
    out_queue[0] = 'E';
    out_queue[1] = 'C';
    out_queue[2] = 'M';
    out_queue[3] = 0x01; // ECM version. For now 0 is the original version and 1 the new version
    // Copy the Streams TOC buffer to the output buffer
    memcpy(out_queue + 4, streams_toc_buffer, streams_toc_buffer_current_ofs);
    // set the current output buffer position
    out_queue_current_ofs = streams_toc_buffer_current_ofs + 4;
    // Copy the Sectors TOC buffer to the output buffer
    memcpy(out_queue + out_queue_current_ofs, sectors_toc_buffer, sectors_toc_buffer_current_ofs);
    // set the current output buffer position
    out_queue_current_ofs += sectors_toc_buffer_current_ofs;

    // Reset the input file and TOC position
    fseeko(in, 0, SEEK_SET);
    in_queue_bytes_available = 0;
    in_queue_current_ofs = 0;
    sectors_toc_buffer_current_ofs = 0;
    // Reset the curtype_count to use it again
    curtype_count = 0;

    //
    // Starting the processing part
    //
    while (!return_code) {
        // Flush the data to disk if there's no space for more sectors
        if((queue_size - out_queue_current_ofs) < 2352) {
            fwrite(out_queue, 1, out_queue_current_ofs, out);

            if (ferror(out)) {
                // Something happen while reading the input file
                printfileerror(in, infilename);

                // Exit with error code
                return_code = 1;
            }

            out_queue_current_ofs = 0;
        }

        // Refill IN queue if necessary
        if(
            !feof(in) &&
            (in_queue_bytes_available < 2352)
        ) {
            // We need to read more data
            size_t willread = queue_size - in_queue_bytes_available;
            
            // If the queue offset 
            if(in_queue_bytes_available > 0 && in_queue_current_ofs > 0) {
                memmove(in_queue, in_queue + in_queue_current_ofs, in_queue_bytes_available);
            }

            in_queue_current_ofs = 0;
            
            if(willread) {
                size_t readed = fread(in_queue + in_queue_bytes_available, 1, willread, in);
                
                if (ferror(in)) {
                    // Something happen while reading the input file
                    printfileerror(in, infilename);

                    // Exit with error code
                    return_code = 1;
                }

                // Compute the crc of the readed data 
                input_edc = sTools.edc_compute(
                    input_edc,
                    in_queue + in_queue_bytes_available,
                    willread
                );

                setcounter_encode(ftello(in));
                in_queue_bytes_available += readed;
            }
        }

        // Getting a new sector type from TOC if required
        if (curtype_count == curtype_total) {
            uint8_t readed_bytes = 0;
            int8_t get_count = sTools.read_type_count(sectors_toc_buffer + sectors_toc_buffer_current_ofs, curtype, curtype_total, readed_bytes);

            if (get_count == -1) {
                // There was an error getting the count (maybe corrupted file)
                return_code = 1;
            }
            else if (!get_count) {
                // End Of TOC reached, so file should have be processed completly
                if (!feof(in) || in_queue_bytes_available) {
                    printf("\n\nThere was an error processing the input file...\n");
                    return_code = 1;
                }

                // Add the CRC to the output buffer
                sTools.put32lsb(out_queue + out_queue_current_ofs, input_edc);
                out_queue_current_ofs += 4;

                break;
            }
            sectors_toc_buffer_current_ofs += readed_bytes;
            curtype_count = 0;
        }

        if (feof(in) && in_queue_bytes_available <= 0){
            printf("Unexpected EOF detected. File or headers might be damaged.");
        }

        if (curtype_total == 0) {
            break;
        }

        // We will clean the sector to keep only the data that we want
        uint16_t output_size = 0;
        sTools.clean_sector(out_queue + out_queue_current_ofs, in_queue + in_queue_current_ofs, curtype, output_size,
            OO_REMOVE_SYNC |
            OO_REMOVE_ADDR |
            OO_REMOVE_MODE |
            OO_REMOVE_BLANKS |
            OO_REMOVE_REDUNDANT_FLAG |
            OO_REMOVE_ECC |
            OO_REMOVE_EDC |
            OO_REMOVE_GAP
        );
        //memcpy(out_queue + out_queue_current_ofs, in_queue + in_queue_current_ofs, 2352);

        in_queue_current_ofs += 2352;
        in_queue_bytes_available -= 2352;
        out_queue_current_ofs += output_size;
        curtype_count++;
    }

    // If there is data in the output buffer, we will flush it first
    if (out_queue_current_ofs) {
        fwrite(out_queue, 1, out_queue_current_ofs, out);
        if (ferror(out)) {
            // Something happen while reading the input file
            printfileerror(in, infilename);
            return_code = 1;
        }
    }

    fclose(in);
    fclose(out);
    free(in_queue);
    free(out_queue);
    free(sectors_toc_buffer);
    free(streams_toc_buffer);
    printf("\n\nFinished!\n");
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
        "Optional:\n"
        "    -d/--compatible\n"
        "           Use the old ecm format compatible with the original tool\n"
        "           Will be ignored when decoding because the version is autodetected\n"
        "    -c/--compression <gzip/lzma>\n"
        "           Compress the stream using the provided algorithm (Not implemented yet)\n"
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

static int8_t unecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
) {
    FILE* in  = NULL;
    FILE* out = NULL;
    uint8_t return_code = 0;

    // CRC calculator
    uint32_t input_edc = 0;

    // Input buffer
    uint8_t* in_queue = NULL;
    size_t in_queue_current_ofs = 0;
    size_t in_queue_bytes_available = 0;

    // Output buffer
    uint8_t* out_queue = NULL;
    size_t out_queue_current_ofs = 0;

    // Sectors TOC buffer
    uint8_t* sectors_toc_buffer = NULL;
    size_t sectors_toc_buffer_current_ofs = 0;

    // Streams TOC buffer
    uint8_t* streams_toc_buffer = NULL;
    size_t streams_toc_buffer_current_ofs = 0;
    sector_tools_stream_types curstreamtype = STST_UNKNOWN; // not a valid type

    //
    // Current sector type (run)
    //
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    uint32_t           curtype_count = 0;
    uint32_t           curtype_total = 0;
    // We will start at sector 00:02:00 which is 0x96 -> 150 / 75 sectors per second
    uint32_t           total_sectors = 0x96;

    // Buffers size
    size_t queue_size = ((size_t)(-1)) - 4095;
    if((unsigned long)queue_size > 0x40000lu) {
        queue_size = (size_t)0x40000lu;
    }

    if (!force_rewrite) {
        out = fopen(outfilename, "rb");
        if (out) {
            printf("Error: %s exists; refusing to overwrite\n", outfilename);
            fclose(out);
            return 1;
        }
    }

    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return 1;
    }

    // Getting the input size to set the progress
    fseeko(in, 0, SEEK_END);
    size_t in_total_size = ftello(in);
    fseeko(in, 0, SEEK_SET);
    // Reset the counters
    resetcounter(in_total_size);

    // Allocate input buffer space
    in_queue = (uint8_t*) malloc(queue_size);
    if(!in_queue) {
        printf("Out of memory\n");
        return 1;
    }

    // Allocate sectors toc buffer space. A few kbytes is enought for most of the situations, but 
    // is not too much memory and is better to be sure
    sectors_toc_buffer = (uint8_t*) malloc(0x80000); // A 512Kb buffer to store the sectors TOC
    if(!sectors_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    // Allocate stream toc buffer space. A few kbytes is enought for most of the situations, but 
    // is not too much memory and is better to be sure
    streams_toc_buffer = (uint8_t*) malloc(0x40000); // A 256Kb buffer to store the stream TOC
    if(!streams_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    // We will fill the buffer to avoid a lot of IOPS reading the headers
    if (fread(in_queue, 1, queue_size, in) != queue_size){
        printfileerror(in, infilename);
        return 1;
    }

    // Reading the streams header into the buffer
    in_queue_current_ofs = 0x04;
    uint8_t c = in_queue[in_queue_current_ofs];
    while (c) {
        streams_toc_buffer[streams_toc_buffer_current_ofs] = c;

        streams_toc_buffer_current_ofs++;
        in_queue_current_ofs++;
        c = in_queue[in_queue_current_ofs];
    }

    // We will add the EOD mark the sectors stream
    streams_toc_buffer[streams_toc_buffer_current_ofs] = 0x00;

    // We will reset the strean to current offset and we will move a
    // further sector in the input stream
    streams_toc_buffer_current_ofs = 0;
    in_queue_current_ofs++;
    
    // Reading the sectors header into the buffer
    c = in_queue[in_queue_current_ofs];
    while (c) {
        sectors_toc_buffer[sectors_toc_buffer_current_ofs] = c;

        sectors_toc_buffer_current_ofs++;
        in_queue_current_ofs++;
        c = in_queue[in_queue_current_ofs];
    }

    // We will add the EOD mark the sectors stream
    sectors_toc_buffer[sectors_toc_buffer_current_ofs] = 0x00;

    // We will reset the strean to current offset and we will move a
    // further sector in the input stream
    sectors_toc_buffer_current_ofs = 0;
    in_queue_current_ofs++;
    // Setting the available bytes
    in_queue_bytes_available = queue_size - in_queue_current_ofs;

    // Initializing the Sector Tools object
    sector_tools sTools = sector_tools();

    //
    // Starting the analyzing part to generate the TOC header
    //
    while (!return_code) {
        // Flush the data to disk if there's no space for more sectors
        if((queue_size - out_queue_current_ofs) < 2352) {
            fwrite(out_queue, 1, out_queue_current_ofs, out);

            if (ferror(out)) {
                // Something happen while reading the input file
                printfileerror(in, infilename);

                // Exit with error code
                return_code = 1;
            }

            out_queue_current_ofs = 0;
        }

        // Refill IN queue if necessary
        if(
            !feof(in) &&
            (in_queue_bytes_available < 2352)
        ) {
            // We need to read more data
            size_t willread = queue_size - in_queue_bytes_available;
            
            // If the queue offset 
            if(in_queue_bytes_available > 0 && in_queue_current_ofs > 0) {
                memmove(in_queue, in_queue + in_queue_current_ofs, in_queue_bytes_available);
            }

            in_queue_current_ofs = 0;
            
            if(willread) {
                size_t readed = fread(in_queue + in_queue_bytes_available, 1, willread, in);
                
                if (ferror(in)) {
                    // Something happen while reading the input file
                    printfileerror(in, infilename);

                    // Exit with error code
                    return_code = 1;
                }

                /*
                // Compute the crc of the readed data 
                input_edc = sTools.edc_compute(
                    input_edc,
                    in_queue + in_queue_bytes_available,
                    willread
                );
                */

                setcounter_decode(ftello(in));
                in_queue_bytes_available += readed;
            }
        }

        // Getting a new sector type from TOC if required
        if (curtype_count == curtype_total) {
            uint8_t readed_bytes = 0;
            int8_t get_count = sTools.read_type_count(sectors_toc_buffer + sectors_toc_buffer_current_ofs, curtype, curtype_total, readed_bytes);

            if (get_count == -1) {
                // There was an error getting the count (maybe corrupted file)
                return_code = 1;
            }
            else if (!get_count) {
                // There is no more data in header. Next 4 bytes might be the CRC
                // Reading it...
                uint32_t crc = sTools.get32lsb(in_queue + in_queue_current_ofs);
                in_queue_current_ofs+= 4;
                in_queue_bytes_available -= 4;

                // End Of TOC reached, so file should have be processed completly
                if (!feof(in) || in_queue_bytes_available) {
                    printf("\n\nThere was an error processing the input file...\n");
                    return_code = 1;
                }

                break;
            }
            sectors_toc_buffer_current_ofs += readed_bytes;
            curtype_count = 0;
        }

        if (feof(in) && in_queue_bytes_available <= 0){
            printf("Unexpected EOF detected. File or headers might be damaged.");
        }

        // We will regenerate the sector
        uint16_t bytes_readed = 0;
        sTools.regenerate_sector(out_queue + out_queue_current_ofs, in_queue + in_queue_current_ofs, curtype, total_sectors, bytes_readed,
            OO_REMOVE_SYNC |
            OO_REMOVE_ADDR |
            OO_REMOVE_MODE |
            OO_REMOVE_BLANKS |
            OO_REMOVE_REDUNDANT_FLAG |
            OO_REMOVE_ECC |
            OO_REMOVE_EDC |
            OO_REMOVE_GAP
        );

        in_queue_current_ofs += bytes_readed;
        in_queue_bytes_available -= bytes_readed;
        out_queue_current_ofs += 2352;
        curtype_count++;
        total_sectors++;
    }

    // If there is data in the output buffer, we will flush it first
    if (out_queue_current_ofs) {
        fwrite(out_queue, 1, out_queue_current_ofs, out);
        if (ferror(out)) {
            // Something happen while reading the input file
            printfileerror(in, infilename);
            return_code = 1;
        }
    }

    fclose(in);
    fclose(out);
    free(in_queue);
    free(out_queue);
    free(sectors_toc_buffer);
    free(streams_toc_buffer);
    printf("\n\nFinished!\n");
    return return_code;
}