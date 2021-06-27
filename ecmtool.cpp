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

    // CRC calculator
    uint32_t input_edc = 0;

    // Sectors TOC buffer
    uint8_t* sectors_toc_buffer = NULL;
    size_t sectors_toc_buffer_current_ofs = 0;
    sectors_toc_buffer = (uint8_t*) malloc(0x80000); // A 512Kb buffer to store the sectors TOC
    if(!sectors_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    // Streams TOC buffer
    uint8_t* streams_toc_buffer = NULL;
    size_t streams_toc_buffer_current_ofs = 0;
    sector_tools_stream_types curstreamtype = STST_UNKNOWN; // not a valid type
    streams_toc_buffer = (uint8_t*) malloc(0x40000); // A 256Kb buffer to store the stream TOC
    if(!streams_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    //
    // Current sector type (run)
    //
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    uint32_t           curtype_count = 0;
    uint32_t           curtype_total = 0;    

    sector_tools sTools = sector_tools();

    // Sector buffer
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    //
    // Starting the analyzing part to generate the TOC header
    //
    while (!return_code) {
        sector_tools_types detected_type;

        if (feof(in) || ftello(in) == in_total_size) {
            // If there are no bytes in queue and EOF was reached, break the loop
            detected_type = STT_UNKNOWN;
        }
        else {
            fread(in_sector, 2352, 1, in);
            detected_type = sTools.detect(in_sector);
        }

        // Update the input file position
        setcounter_analyze(ftello(in));

        if(
            (detected_type == curtype) &&
            (curtype_count <= 0x7FFFFFFF) // avoid overflow
        ) {
            // Same type as last sector
            curtype_count++;
        }
        else {
            uint8_t generated_bytes;
            // Checking if the stream type is different than the last one
            sector_tools_stream_types stream_type = sTools.detect_stream(curtype);

            if (curstreamtype == STST_UNKNOWN) {
                // First stream position. We need the end of stream, so we will just save the type
                curstreamtype = stream_type;
            }
            else if (stream_type != curstreamtype) {
                // nTH stream, saving the position as "END" of the last stream
                uint8_t generated_bytes;
                uint32_t current_in = ftello(in);
                sTools.write_stream_type_count(
                    streams_toc_buffer + streams_toc_buffer_current_ofs,
                    STC_NONE,
                    curstreamtype,
                    current_in,
                    generated_bytes
                );
                streams_toc_buffer_current_ofs += generated_bytes;
                curstreamtype = stream_type;
            }

            if(curtype_count > 0) {
                // Generate the sector mode data
                sTools.write_type_count(
                    sectors_toc_buffer + sectors_toc_buffer_current_ofs,
                    curtype,
                    curtype_count,
                    generated_bytes
                );
                sectors_toc_buffer_current_ofs += generated_bytes; 
            }

            curtype = detected_type;
            curtype_count = 1;
        }

        // if curtype is unknown, then the EOF is reached
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

            break;
        }
    }

    // Once all is processed, we will store the last stream End Of Stream
    uint8_t generated_bytes = 0;
    sTools.write_stream_type_count(
        streams_toc_buffer + streams_toc_buffer_current_ofs,
        STC_NONE, // None for now
        curstreamtype,
        ftello(in),
        generated_bytes
    );
    streams_toc_buffer_current_ofs += generated_bytes;
    // Write the End of Data to Streams TOC buffer
    sTools.write_stream_type_count(
        streams_toc_buffer + streams_toc_buffer_current_ofs,
        STC_NONE,
        STST_AUDIO, // The function will extract 1 from this, so to store 0 must be 1 (STST_AUDIO)
        0,
        generated_bytes
    );
    streams_toc_buffer_current_ofs++;

    // Once the file is analyzed and we know the TOC, we will process al the data
    //
    //printf("Writting header and TOC data to output buffer\n");
    // Add the Header to the output file
    uint8_t header[4];
    header[0] = 'E';
    header[1] = 'C';
    header[2] = 'M';
    header[3] = 0x01; // ECM version. For now 0 is the original version and 1 the new version
    fwrite(header, 4, 1, out);

    // Write the Streams TOC buffer to the output file
    fwrite(streams_toc_buffer, streams_toc_buffer_current_ofs, 1, out);
    // Write the Sectors TOC buffer to the output file
    fwrite(sectors_toc_buffer, sectors_toc_buffer_current_ofs, 1, out);

    // Reset the input file and TOC position
    fseeko(in, 0, SEEK_SET);
    sectors_toc_buffer_current_ofs = 0;
    // Reset the curtype_count to use it again
    curtype_count = 0;

    //
    // Starting the processing part
    //
    while (!return_code) {
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
                if (!feof(in) && ftello(in) != in_total_size) {
                    printf("\n\nThere was an error processing the input file...\n");
                    return_code = 1;
                }

                // Add the CRC to the output buffer
                uint8_t crc[4];
                sTools.put32lsb(crc, input_edc);
                fwrite(crc, 4, 1, out);
                break;
            }
            sectors_toc_buffer_current_ofs += readed_bytes;
            curtype_count = 0;
        }

        if (feof(in)){
            printf("Unexpected EOF detected. File or headers might be damaged.");
        }

        if (curtype_total == 0) {
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
        sTools.clean_sector(out_sector, in_sector, curtype, output_size,
            OO_REMOVE_SYNC |
            OO_REMOVE_ADDR |
            OO_REMOVE_MODE |
            OO_REMOVE_BLANKS |
            OO_REMOVE_REDUNDANT_FLAG |
            OO_REMOVE_ECC |
            OO_REMOVE_EDC |
            OO_REMOVE_GAP
        );

        curtype_count++;
        fwrite(out_sector, output_size, 1, out);
        setcounter_encode(ftello(in));
    }

    fclose(in);
    fflush(out);
    fclose(out);
    free(in_buffer);
    free(out_buffer);
    free(sectors_toc_buffer);
    free(streams_toc_buffer);
    printf("\n\nFinished!\n");
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
    fseeko(in, 0, SEEK_SET);
    // Reset the counters
    resetcounter(in_total_size);
    // Open output file
    out = fopen(outfilename, "wb");
    if (!in) {
        printfileerror(out, outfilename);
        return 1;
    }
    setvbuf(out, out_buffer, _IOFBF, buffer_size);

    // CRC calculator
    uint32_t original_edc = 0;
    uint32_t output_edc = 0;

    // Sectors TOC buffer
    uint8_t* sectors_toc_buffer = NULL;
    size_t sectors_toc_buffer_current_ofs = 0;
    sectors_toc_buffer = (uint8_t*) malloc(0x80000); // A 512Kb buffer to store the sectors TOC
    if(!sectors_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    // Streams TOC buffer
    uint8_t* streams_toc_buffer = NULL;
    size_t streams_toc_buffer_current_ofs = 0;
    sector_tools_stream_types curstreamtype = STST_UNKNOWN; // not a valid type
    size_t streams_eos_position = 0;
    streams_toc_buffer = (uint8_t*) malloc(0x40000); // A 256Kb buffer to store the stream TOC
    if(!streams_toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    //
    // Current sector type (run)
    //
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    uint32_t           curtype_count = 0;
    uint32_t           curtype_total = 0;
    // We will start at sector 00:02:00 which is 0x96 -> 150 / 75 sectors per second
    uint32_t           total_sectors = 0x96;

    // Keep a track of the last input position
    size_t last_in_position = 0;

    // Reading the streams header into the buffer
    fseeko(in, 0x04, SEEK_SET);
    uint8_t c[1];
    fread(c, 1, 1, in);
    while (c[0]) {
        streams_toc_buffer[streams_toc_buffer_current_ofs] = c[0];

        streams_toc_buffer_current_ofs++;
        fread(c, 1, 1, in);
    }

    // We will add the EOD mark the sectors stream
    streams_toc_buffer[streams_toc_buffer_current_ofs] = 0x00;

    // We will reset the strean to current offset and we will move a
    // further sector in the input stream
    streams_toc_buffer_current_ofs = 0;
    
    // Reading the sectors header into the buffer
    fread(c, 1, 1, in);
    while (c[0]) {
        sectors_toc_buffer[sectors_toc_buffer_current_ofs] = c[0];

        sectors_toc_buffer_current_ofs++;
        fread(c, 1, 1, in);
    }

    // We will add the EOD mark the sectors stream
    sectors_toc_buffer[sectors_toc_buffer_current_ofs] = 0x00;

    // We will reset the strean to current offset and we will move a
    // further sector in the input stream
    sectors_toc_buffer_current_ofs = 0;

    // Initializing the Sector Tools object
    sector_tools sTools = sector_tools();

    // Sector buffer
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    // Set the current positions after the headers as last position
    last_in_position = ftello(in);

    //
    // Starting the decoding part to generate the output file
    //
    while (!return_code) {
        // Before we will calculate the end of stream
        uint32_t current_stream_end_position = 0;
        uint8_t readed_bytes = 0;
        sector_tools_compression compression = STC_NONE;
        if (ftello(out) == streams_eos_position) {
            // Get the new stream data
            int8_t status = sTools.read_stream_type_count(
                streams_toc_buffer + streams_toc_buffer_current_ofs,
                compression,
                curstreamtype,
                current_stream_end_position,
                readed_bytes
            );
            streams_toc_buffer_current_ofs += readed_bytes;

            // We will set the new stream end position
            streams_eos_position = current_stream_end_position;
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
                uint8_t buffer_edc[4];
                fread(buffer_edc, 4, 1, in);
                original_edc = sTools.get32lsb(buffer_edc);

                // End Of TOC reached, so file should have be processed completly
                if (!feof(in) && ftello(in) != in_total_size) {
                    printf("\n\nThere was an error processing the input file...\n");
                    return_code = 1;
                }

                break;
            }
            sectors_toc_buffer_current_ofs += readed_bytes;
            curtype_count = 0;
        }

        if (feof(in)){
            printf("Unexpected EOF detected. File or headers might be damaged.");
        }

        // We will regenerate the sector
        uint16_t bytes_readed = 0;
        fread(in_sector, 2352, 1, in);
        sTools.regenerate_sector(out_sector, in_sector, curtype, total_sectors, bytes_readed,
            OO_REMOVE_SYNC |
            OO_REMOVE_ADDR |
            OO_REMOVE_MODE |
            OO_REMOVE_BLANKS |
            OO_REMOVE_REDUNDANT_FLAG |
            OO_REMOVE_ECC |
            OO_REMOVE_EDC |
            OO_REMOVE_GAP
        );
        fwrite(out_sector, 2352, 1, out);
        // Compute the crc of the written data 
        output_edc = sTools.edc_compute(
            output_edc,
            out_sector,
            2352
        );

        last_in_position += bytes_readed;
        fseeko(in, last_in_position, SEEK_SET);
        setcounter_decode(ftello(in));
        curtype_count++;
        total_sectors++;
    }

    fflush(out);
    fclose(in);
    fclose(out);
    free(in_buffer);
    free(out_buffer);
    free(sectors_toc_buffer);
    free(streams_toc_buffer);
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