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

    // The in file is an ECM file, so will be decoded
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
            printf("An ECM file was detected, so will be decoded\n");
            decode = true;

            if (fgetc(in) == 0x00) {
                // The file is in the old format, so compatibility will be enabled
                compatible = true;
            }
        }
        else {
            printf("A BIN file was detected, so will be encoded\n");
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

    ecmify(infilename, outfilename, force_rewrite);

    


/*
    // The in file is an ECM file, so will be decoded
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
            printf("An ECM file was detected, so will be decoded\n");
            decode = true;

            if (fgetc(in) == 0x00) {
                // The file is in the old format, so compatibility will be enabled
                compatible = true;
            }
        }
        else {
            printf("A BIN file was detected, so will be encoded\n");
        }
        fclose(in);
    }

    // If not output filename was provided, we will generate one
    // based in the original filename.
    if (outfilename == NULL) {
        tempfilename = malloc(strlen(infilename) + 7);
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

    //
    // Initialize the ECC/EDC tables
    //
    eccedc_init();

    //
    // Go!
    //
    uint8_t status = 0;
    if(decode) {
        if (compatible) {
            // Using the old method
            if(old_unecmify(infilename, outfilename)) { status = 1; }
        }
        else {
            // Using the new method
            if(unecmify(infilename, outfilename)) { status = 1; }
        }
    } else {
        if (compatible) {
            // Using the old method
            if(old_ecmify(infilename, outfilename)) { status = 1; }
        }
        else {
            // Using the new method
            if(ecmify(infilename, outfilename)) { status = 1; }
        }
    }

    //
    // Success
    //
    if(tempfilename) { free(tempfilename); }
    return status;
    */
}


static int8_t ecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
) {
    FILE* in  = NULL;
    FILE* out = NULL;

    // Input buffer
    uint8_t* in_queue = NULL;
    size_t in_queue_current_ofs = 0;
    size_t in_queue_bytes_available = 0;

    // Output buffer
    uint8_t* out_queue = NULL;
    size_t out_queue_current_ofs = 0;

    // TOC buffer
    uint8_t* toc_buffer = NULL;
    size_t toc_buffer_current_ofs = 0;

    //
    // Current sector type (run)
    //
    int8_t   curtype = -1; // not a valid type
    uint32_t curtype_count = 0;
    //off_t    curtype_in_start = 0;

    //off_t input_file_length;
    //off_t input_bytes_checked = 0;
    //off_t input_bytes_queued  = 0;

    //off_t typetally[4] = {0,0,0,0};

    /* Will be returned by the cleanup function depending of which level
    static const size_t sectorsize[] = {
        0,      // UNKNOWN
        2352,   // CDDA
        0,      // CDDA GAP
        2048,   // MODE1
        0,      // MODE1 GAP
        2336,   // MODE2
        0,      // MODE2 GAP
        2052,   // MODE2 XA 1
        4,      // MODE2 XA 1 GAP
        2328,   // MODE2 XA 2
        4,      // MODE2 XA 2 GAP
    };
    */

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
    if (ftello(in) % 2352) {
        printf("ERROR: The input file doesn't appear to be a CD-ROM image\n");
        printf("       This program only allows to process CD-ROM images\n");

        fclose(in);
        return 1;
    }
    fseeko(in, 0, SEEK_SET);

    out = fopen(outfilename, "wb");
    if (!out) {
        printfileerror(out, infilename);
        return 1;
    }

    //
    // Allocate input buffer space
    //
    in_queue = (uint8_t*) malloc(queue_size);
    if(!in_queue) {
        printf("Out of memory\n");
        return 1;
    }
    //
    // Allocate toc buffer space. A few kbytes is enought for most of the situations, but 
    // is not too much memory and is better to be sure
    //
    toc_buffer = (uint8_t*) malloc(0x500000); // A 5MB buffer to store the TOC
    if(!toc_buffer) {
        printf("Out of memory\n");
        return 1;
    }

    sector_tools sTools = sector_tools();

    //
    //
    // Starting the analyzing part to generate the TOC header
    //
    //
    for(;;) {
        int8_t detected_type;

        //
        // Refill queue if necessary
        //
        if(
            !feof(in) &&
            (in_queue_bytes_available < 2352)
        ) {
            //
            // We need to read more data
            //
            size_t willread = queue_size - in_queue_bytes_available;
            
            // If the queue offset 
            if(in_queue_bytes_available > 0 && in_queue_current_ofs > 0) {
                memmove(in_queue, in_queue + in_queue_current_ofs, in_queue_bytes_available);
            }

            in_queue_current_ofs = 0;
            
            if(willread) {
                //setcounter_analyze(input_bytes_queued);

                size_t readed = fread(in_queue + in_queue_bytes_available, 1, willread, in);
                
                if (ferror(in)) {
                    // Something happen while reading the input file
                    printfileerror(in, infilename);

                    // We will close both files before exit
                    if(in    != NULL) { fclose(in ); }
                    if(out   != NULL) { fclose(out); }
                    free(in_queue);
                    free(out_queue);

                    // Exit with error code
                    return 1;
                }

                in_queue_bytes_available += readed;
            }
        }

        if (in_queue_bytes_available == 0) {
            // If analyze step is complete, just flush the data and exit the loop
            detected_type = -1;
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
            //
            // Same type as last sector
            //
            curtype_count++;
        }
        else {
            if(curtype_count > 0) {
                // Generate the sector mode data
                printf("Generating curtype data. Type %d - count %d - pos %d\n", curtype, curtype_count, ftello(in));
                uint8_t generated_bytes;
                sTools.write_type_count(
                    toc_buffer + toc_buffer_current_ofs,
                    curtype,
                    curtype_count,
                    generated_bytes
                );
                toc_buffer_current_ofs += generated_bytes;
            }

            curtype = detected_type;
            curtype_count = 1;
        }

        //
        // if curtype is negative at this point, then the EOF is reached
        //
        if(curtype < 0) {
            // Write the End of Data to TOC buffer
            uint8_t generated_bytes;
            sTools.write_type_count(
                toc_buffer + toc_buffer_current_ofs,
                0,
                0,
                generated_bytes
            );
            toc_buffer_current_ofs++;
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

    // Add the Header to the output buffer
    out_queue[0] = 'E';
    out_queue[1] = 'C';
    out_queue[2] = 'M';
    out_queue[3] = 0x01; // ECM version. For now 0 is the original version and 1 the new version
    // Copy the TOC buffer to the output buffer
    memcpy(out_queue + 4, toc_buffer, toc_buffer_current_ofs);
    // set the current output buffer position
    out_queue_current_ofs = toc_buffer_current_ofs + 4;
    // Reset the input file position
    fseeko(in, 0, SEEK_SET);
    in_queue_bytes_available = 0;
    in_queue_current_ofs = 0;

    printf("Writting output file\n");

    //
    //
    // Starting the processing part
    //
    //
    for (;;) {
        // Flush the data to disk if there's no space for more sectors
        if((queue_size - out_queue_current_ofs) < 2352) {
            fwrite(out_queue, 1, out_queue_current_ofs, out);

            if (ferror(out)) {
                // Something happen while reading the input file
                printfileerror(in, infilename);

                // We will close both files before exit
                if(in    != NULL) { fclose(in ); }
                if(out   != NULL) { fclose(out); }
                free(in_queue);
                free(out_queue);
                free(toc_buffer);

                // Exit with error code
                return 1;
            }

            out_queue_current_ofs = 0;
        }

        //
        // Refill IN queue if necessary
        //
        if(
            !feof(in) &&
            (in_queue_bytes_available < 2352)
        ) {
            //
            // We need to read more data
            //
            size_t willread = queue_size - in_queue_bytes_available;
            
            // If the queue offset 
            if(in_queue_bytes_available > 0 && in_queue_current_ofs > 0) {
                memmove(in_queue, in_queue + in_queue_current_ofs, in_queue_bytes_available);
            }

            in_queue_current_ofs = 0;
            
            if(willread) {
                //setcounter_analyze(input_bytes_queued);

                size_t readed = fread(in_queue + in_queue_bytes_available, 1, willread, in);
                
                if (ferror(in)) {
                    // Something happen while reading the input file
                    printfileerror(in, infilename);

                    // We will close both files before exit
                    if(in    != NULL) { fclose(in ); }
                    if(out   != NULL) { fclose(out); }
                    free(in_queue);
                    free(out_queue);
                    free(toc_buffer);

                    // Exit with error code
                    return 1;
                }

                /*
                input_edc = edc_compute(
                    input_edc,
                    queue + queue_bytes_available,
                    willread
                );
                */

                in_queue_bytes_available += readed;
            }
        }

        if (in_queue_bytes_available == 0) {
            uint8_t return_code = 0;
            // If there is data in output buffer, we will clear it first
            if (out_queue_current_ofs) {
                fwrite(out_queue, 1, out_queue_current_ofs, out);
                if (ferror(out)) {
                    // Something happen while reading the input file
                    printfileerror(in, infilename);
                    return_code = 1;
                }
            }

            // We will close both files before exit
            if(in    != NULL) { fclose(in ); }
            if(out   != NULL) { fclose(out); }
            free(in_queue);
            free(out_queue);
            free(toc_buffer);

            printf("Finished!\n");

            return return_code;
        }

        // For now we will just copy the data from a buffer to another for testing
        uint16_t output_size = 0;
        sector_tools_types type = STT_CDDA;
        sTools.clean_sector(out_queue + out_queue_current_ofs, in_queue + in_queue_current_ofs, type, output_size,
            OO_REMOVE_SYNC |
            OO_REMOVE_ADDR |
            OO_REMOVE_MODE |
            OO_REMOVE_BLANKS |
            OO_REMOVE_REDUNDANT_FLAG |
            OO_REMOVE_ECC |
            OO_REMOVE_EDC |
            OO_REMOVE_GAP
        );
        printf("Tamaño sector: %d\n", output_size);
        //memcpy(out_queue + out_queue_current_ofs, in_queue + in_queue_current_ofs, 2352);

        in_queue_current_ofs += 2352;
        in_queue_bytes_available -= 2352;
        out_queue_current_ofs += output_size;
    }

    fclose(in);
    fclose(out);
    free(in_queue);
    free(out_queue);
    return 0;
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