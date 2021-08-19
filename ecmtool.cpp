////////////////////////////////////////////////////////////////////////////////
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

#include "ecmtool.h"

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

    int return_code = 0;

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
                else if (strcmp("lz4", optarg) == 0) {
                    audio_compression = C_LZ4;
                }
                else if (strcmp("flac", optarg) == 0) {
                    audio_compression = C_FLAC;
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
                else if (strcmp("lz4", optarg) == 0) {
                    data_compression = C_LZ4;
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
        return_code = unecmify(infilename, outfilename, force_rewrite);
    }
    else {
        return_code = ecmify(
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

    // Free the tempfilename resource
    if (tempfilename) {
        free(tempfilename);
    }

    return return_code;
}


static ecmtool_return_code ecmify(
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
    ecmtool_return_code return_code = ECMTOOL_OK;

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
            printf("Error: %s exists; refusing to overwrite. Use the -f argument to force the overwrite.\n", outfilename);
            fclose(out);
            return ECMTOOL_FILE_WRITE_ERROR;
        }
    }

    // Buffers initialization
    // Allocate input buffer space
    char* in_buffer = NULL;
    in_buffer = (char*) malloc(BUFFER_SIZE);
    if(!in_buffer) {
        printf("Out of memory\n");
        return ECMTOOL_BUFFER_MEMORY_ERROR;
    }
    // Allocate output buffer space
    char* out_buffer = NULL;
    out_buffer = (char*) malloc(BUFFER_SIZE);
    if(!out_buffer) {
        printf("Out of memory\n");
        return ECMTOOL_BUFFER_MEMORY_ERROR;
    }

    // Open input file
    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return ECMTOOL_FILE_READ_ERROR;
    }
    setvbuf(in, in_buffer, _IOFBF, BUFFER_SIZE);

    // We will check if image is a CD-ROM
    // CD-ROMS sectors are 2352 bytes size and are always filled, so image size must be multiple
    fseeko(in, 0, SEEK_END);
    size_t in_total_size = ftello(in);
    if (in_total_size % 2352) {
        printf("ERROR: The input file doesn't appear to be a CD-ROM image\n");
        printf("       This program only allows to process CD-ROM images\n");

        fclose(in);
        return ECMTOOL_FILE_READ_ERROR;
    }
    fseeko(in, 0, SEEK_SET);
    // Reset the counters
    resetcounter(in_total_size);

    // Open output file
    out = fopen(outfilename, "wb");
    if (!out) {
        printfileerror(out, infilename);
        return ECMTOOL_FILE_WRITE_ERROR;
    }
    setvbuf(out, out_buffer, _IOFBF, BUFFER_SIZE);

    // Sectors TOC
    SECTOR *sectors_toc = new SECTOR[0xFFFFF];
    SEC_STR_SIZE sectors_toc_count = {0, 0};

    // Streams TOC
    STREAM *streams_toc = new STREAM[0xFFFFF];
    SEC_STR_SIZE streams_toc_count = {0, 0};

    // Sector Tools object
    sector_tools sTools = sector_tools();

    return_code = disk_analyzer (
        &sTools,
        in,
        in_total_size,
        streams_toc,
        &streams_toc_count,
        sectors_toc,
        &sectors_toc_count,
        data_compression,
        audio_compression
    );

    // Convert the headers to an script to be easily followed by the encoder
    std::vector<STREAM_SCRIPT> streams_script;
    return_code = task_maker (
        streams_toc,
        streams_toc_count,
        sectors_toc,
        sectors_toc_count,
        streams_script
    );

    if(!return_code) {
        // Compress the sectors header.  Sectors count is base 0, so one will be added to the size calculation
        uint32_t sectors_toc_size = (sectors_toc_count.count + 1) * sizeof(struct SECTOR);
        char* sectors_toc_c_buffer = (char*) malloc(sectors_toc_size * 2);
        if(!sectors_toc_c_buffer) {
            printf("Out of memory\n");
            return ECMTOOL_BUFFER_MEMORY_ERROR;
        }

        if(!return_code) {
            uint32_t compressed_size = sectors_toc_size * 2;
            compress_header((uint8_t*)sectors_toc_c_buffer, compressed_size, (uint8_t*)sectors_toc, sectors_toc_size, 9);

            // Set the size of header. Streams count is base 0, so one will be added to the size calculation
            streams_toc_count.size = (streams_toc_count.count + 1) * sizeof(struct STREAM);
            sectors_toc_count.size = compressed_size;

            // Once the file is analyzed and we know the TOC, we will process al the data
            //
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
            fwrite(streams_toc, streams_toc_count.size, 1, out);
            // Write the Sectors TOC to the output file
            fwrite(&sectors_toc_count, sizeof(sectors_toc_count), 1, out);
            fwrite(sectors_toc_c_buffer, compressed_size, 1, out);

            // Reset the input file position
            fseeko(in, 0, SEEK_SET);
        }

        // Free the compressed buffer
        if(sectors_toc_c_buffer) {
            free(sectors_toc_c_buffer);
        }
    }

    // CRC calculation to check the decoded stream
    uint32_t input_edc = 0;

    //
    // Starting the processing part
    //
    uint32_t sectors_type[11] = {};
    return_code = disk_encode (
        &sTools,
        in,
        out,
        streams_script,
        compression_level,
        extreme_compression,
        seekable,
        sectors_per_block,
        optimizations,
        sectors_type,
        input_edc
    );

    // Updating streams header
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        streams_toc[i].out_end_position = streams_script[i].stream_data.out_end_position;
    }

    // Add the CRC to the output file
    uint8_t crc[4];
    sTools.put32lsb(crc, input_edc);
    fwrite(crc, 4, 1, out);

    // Rewrite the streams header to store the new data (end position in output file)
    fseeko(out, 5 + sizeof(optimizations) + sizeof(streams_toc_count), SEEK_SET);
    // Write the streams header. The count is base 0, so one will be added for the calculation
    fwrite(streams_toc, streams_toc_count.size, 1, out);

    // End Of TOC reached, so file should have be processed completly
    if (ftello(in) != in_total_size) {
        printf("\n\nThere was an error processing the input file...\n");
        printfileerror(in, infilename);
        return ECMTOOL_FILE_READ_ERROR;
    }

    fseeko(out, 0, SEEK_END);
    summary(sectors_type, optimizations, sTools, ftello(out));

    printf("Exiting...\n");

    //sTools.close();
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


static ecmtool_return_code unecmify(
    const char* infilename,
    const char* outfilename,
    const bool force_rewrite
) {
    // IN/OUT files definition
    FILE* in  = NULL;
    FILE* out = NULL;
    ecmtool_return_code return_code = ECMTOOL_OK;

    if (!force_rewrite) {
        out = fopen(outfilename, "rb");
        if (out) {
            printf("Error: %s exists; refusing to overwrite. Use the -f argument to force the overwrite.\n", outfilename);
            fclose(out);
            return ECMTOOL_FILE_WRITE_ERROR;
        }
    }

    // Buffers initialization
    // Allocate input buffer space
    char* in_buffer = NULL;
    in_buffer = (char*) malloc(BUFFER_SIZE);
    if(!in_buffer) {
        printf("Out of memory\n");
        return ECMTOOL_BUFFER_MEMORY_ERROR;
    }
    // Allocate output buffer space
    char* out_buffer = NULL;
    out_buffer = (char*) malloc(BUFFER_SIZE);
    if(!out_buffer) {
        printf("Out of memory\n");
        return ECMTOOL_BUFFER_MEMORY_ERROR;
    }

    // Open input file
    in = fopen(infilename, "rb");
    if (!in) {
        printfileerror(in, infilename);
        return ECMTOOL_FILE_READ_ERROR;
    }
    setvbuf(in, in_buffer, _IOFBF, BUFFER_SIZE);
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
        return ECMTOOL_FILE_WRITE_ERROR;
    }
    setvbuf(out, out_buffer, _IOFBF, BUFFER_SIZE);

    // Get the options used at file creation
    optimization_options optimizations;
    fread(&optimizations, sizeof(optimizations), 1, in);

    // Streams TOC
    SEC_STR_SIZE streams_toc_count = {0, 0};
    fread(&streams_toc_count, sizeof(streams_toc_count), 1, in);
    STREAM *streams_toc = new STREAM[streams_toc_count.count + 1];
    if (sizeof(streams_toc) > streams_toc_count.size) {
        // There was an error in the header
        return ECMTOOL_CORRUPTED_HEADER;
    }
    fread(streams_toc, streams_toc_count.size, 1, in);

    // Sectors TOC
    SEC_STR_SIZE sectors_toc_count = {0, 0};
    fread(&sectors_toc_count, sizeof(sectors_toc_count), 1, in);
    SECTOR *sectors_toc = new SECTOR[sectors_toc_count.count + 1];
    char* sectors_toc_c_buffer = (char*) malloc(sectors_toc_count.size);
    if(!sectors_toc_c_buffer) {
        printf("Out of memory\n");
        return_code = ECMTOOL_BUFFER_MEMORY_ERROR;
    }

    // Decompress the header
    uint32_t out_size = (sectors_toc_count.count + 1) * sizeof(struct SECTOR);
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
            return_code = ECMTOOL_CORRUPTED_HEADER;
        }
    }

    // Free the compressed buffer
    if(sectors_toc_c_buffer) {
        free(sectors_toc_c_buffer);
    }

    // Initializing the Sector Tools object
    sector_tools sTools = sector_tools();

    // Convert the headers to an script to be easily followed by the decoder
    std::vector<STREAM_SCRIPT> streams_script;
    return_code = task_maker (
        streams_toc,
        streams_toc_count,
        sectors_toc,
        sectors_toc_count,
        streams_script
    );

    if (return_code) {
        return ECMTOOL_CORRUPTED_STREAM;
    }

    return_code = disk_decode (
        &sTools,
        in,
        out,
        streams_script,
        optimizations
    );

    // Flushing data and closing files
    fflush(out);
    fclose(in);
    fclose(out);
    // Freeing reserved memory
    free(in_buffer);
    free(out_buffer);
    delete[] sectors_toc;
    delete[] streams_toc;
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
        "    -a/--acompression <zlib/lzma/lz4/flac>\n"
        "           Enable audio compression\n"
        "    -d/--dcompression <zlib/lzma/lz4>\n"
        "           Enable data compression\n"
        "    -c/--clevel <0-9>\n"
        "           Compression level between 0 and 9\n"
        "    -e/--extreme-compression\n"
        "           Enables extreme compression mode for LZMA/FLAC (can be very slow)\n"
        "    -s/--seekable\n"
        "           Create a seekable file. Reduce the compression ratio but\n"
        "           but allow to seek into the stream.\n"
        "    -p/--sectors_per_block <sectors>\n"
        "           Add a end of block mark every X sectors in a seekable file. Max 255.\n"
        "    -f/--force\n"
        "           Force to ovewrite the output file\n"
    );
}


static void summary(uint32_t * sectors, optimization_options optimizations, sector_tools sTools, size_t compressed_size) {
    uint16_t optimized_sector_sizes[11];

    // Calculate the size per sector type
    for (uint8_t i = 1; i < 11; i++) {
        size_t bytes_to_read = 0;
        // Getting the sector size prior to read, to read the real sector size and avoid to fseek every time
        sTools.encoded_sector_size(
            (sector_tools_types)i,
            bytes_to_read,
            optimizations
        );
        optimized_sector_sizes[i] = bytes_to_read;
    }

    // Total sectors
    uint32_t total_sectors = 0;
    for (uint8_t i = 1; i < 11; i++) {
        total_sectors += sectors[i];
    }

    // Total size
    size_t total_size = total_sectors * 2352;

    // ECM size without compression
    size_t ecm_size = 0;
    for (uint8_t i = 1; i < 11; i++) {
        ecm_size += sectors[i] * optimized_sector_sizes[i];
    }

    printf("\n\n");
    printf(" ECM cleanup sumpary\n");
    printf("------------------------------------------------------------\n");
    printf(" Type               Sectors         In Size        Out Size\n");
    printf("------------------------------------------------------------\n");
    printf("CDDA ............... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[1], MB(sectors[1] * 2352), MB(sectors[1] * optimized_sector_sizes[1])); 
    printf("CDDA Gap ........... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[2], MB(sectors[2] * 2352), MB(sectors[2] * optimized_sector_sizes[2]));
    printf("Mode 1 ............. %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[3], MB(sectors[3] * 2352), MB(sectors[3] * optimized_sector_sizes[3]));
    printf("Mode 1 Gap ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[4], MB(sectors[4] * 2352), MB(sectors[4] * optimized_sector_sizes[4]));
    printf("Mode 2 ............. %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[5], MB(sectors[5] * 2352), MB(sectors[5] * optimized_sector_sizes[5]));
    printf("Mode 2 Gap ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[6], MB(sectors[6] * 2352), MB(sectors[6] * optimized_sector_sizes[6]));
    printf("Mode 2 XA1 ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[7], MB(sectors[7] * 2352), MB(sectors[7] * optimized_sector_sizes[7]));
    printf("Mode 2 XA1 Gap ..... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[8], MB(sectors[8] * 2352), MB(sectors[8] * optimized_sector_sizes[8]));
    printf("Mode 2 XA2 ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[9], MB(sectors[9] * 2352), MB(sectors[9] * optimized_sector_sizes[9]));
    printf("Mode 2 XA2 Gap ..... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors[10], MB(sectors[10] * 2352), MB(sectors[10] * optimized_sector_sizes[10]));
    printf("-------------------------------------------------------------\n");
    printf("Total .............. %6d ...... %6.2fMb ...... %6.2fMb\n", total_sectors, MB(total_size), MB(ecm_size));
    printf("ECM reduction (input vs ecm) ..................... %2.2f%%\n", (1.0 - ((float)ecm_size / total_size)) * 100);
    printf("\n\n");

    printf(" Compression Sumary\n");
    printf("-------------------------------------------------------------\n");
    printf("Compressed size (output) ............... %3.2fMB\n", MB(compressed_size));
    printf("Compression ratio (ecm vs output)....... %2.2f%%\n", abs((1.0 - ((float)compressed_size / ecm_size)) * 100));
    printf("\n\n");

    printf(" Output summary\n");
    printf("-------------------------------------------------------------\n");
    printf("Total reduction (input vs output) ...... %2.2f%%\n", abs((1.0 - ((float)compressed_size / total_size)) * 100));
}


static ecmtool_return_code disk_analyzer (
    sector_tools * sTools,
    FILE * image_file,
    size_t image_file_size,
    STREAM * streams_toc,
    SEC_STR_SIZE * streams_toc_size,
    SECTOR * sectors_toc,
    SEC_STR_SIZE * sectors_toc_size,
    sector_tools_compression data_compression,
    sector_tools_compression audio_compression
) {
    // Sector count
    size_t sectors_count = image_file_size / 2352;

    // Current sector type and counters initialization
    sector_tools_types curtype = STT_UNKNOWN; // not a valid type
    // Bad stream type to create a new one at startup
    sector_tools_stream_types curstreamtype = STST_UNKNOWN;

    // Sector buffer
    uint8_t in_sector[2352];

    // Loop through all the sectors
    for (size_t i = 1; i <= sectors_count; i++) {
        // Read a sector
        if (fread(in_sector, 2352, 1, image_file)) {
            // Update the input file position
            setcounter_analyze(ftello(image_file));

            sector_tools_types detected_type = sTools->detect(in_sector);
            if (curtype == STT_UNKNOWN) {
                // Initialize the first sector type
                sectors_toc[sectors_toc_size->count].mode = detected_type;
                sectors_toc[sectors_toc_size->count].sector_count = 1;
                curtype = detected_type;
            }
            else if (detected_type == curtype) {
                // Sector type is the same so will be added
                sectors_toc[sectors_toc_size->count].sector_count++;
            }
            else {
                // Checking if the stream type is also different than the last one
                sector_tools_stream_types stream_type = sTools->detect_stream(detected_type);

                // Select the compression depending of the stream type and the user options
                sector_tools_compression current_compression;
                if (stream_type == STST_AUDIO) {
                    current_compression = audio_compression;
                }
                else {
                    current_compression = data_compression;
                }

                if (curstreamtype == STST_UNKNOWN) {
                    // First stream position. Set the known stream data
                    streams_toc[streams_toc_size->count].type = stream_type - 1;
                    streams_toc[streams_toc_size->count].compression = current_compression;
                    curstreamtype = stream_type;
                }
                else if (stream_type != curstreamtype) {
                    // Set the end sector of the current stream (was the previous sector)
                    streams_toc[streams_toc_size->count].end_sector = i - 1;
                    // Add one stream to the count
                    streams_toc_size->count++;
                    streams_toc[streams_toc_size->count].type = stream_type - 1;
                    curstreamtype = stream_type;
                }

                // Set the new sector toc position and initialize it
                sectors_toc_size->count++;
                sectors_toc[sectors_toc_size->count].mode = detected_type;
                streams_toc[streams_toc_size->count].compression = current_compression;
                sectors_toc[sectors_toc_size->count].sector_count = 1;
                curtype = detected_type;
            }
        }
        else {
            // There was an eror reading the new sector
            return ECMTOOL_FILE_READ_ERROR;
        }
    }

    // Setting the last stream sector_count to total sectors
    streams_toc[streams_toc_size->count].end_sector = sectors_count;

    return ECMTOOL_OK;
}


static ecmtool_return_code disk_encode (
    sector_tools * sTools,
    FILE * image_in,
    FILE * ecm_out,
    std::vector<STREAM_SCRIPT> & streams_script,
    uint8_t compression_level,
    bool extreme_compression,
    bool seekable,
    uint8_t sectors_per_block,
    optimization_options optimizations,
    uint32_t * sectors_type,
    uint32_t & input_edc
) {
    // Sectors buffers
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    // Stream processing
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        // Compressor object
        compressor *compobj = NULL;
        // Buffer object
        uint8_t* comp_buffer = NULL;

        // Initialize the compressor and the buffer if required
        if (streams_script[i].stream_data.compression) {
            // Set compression level with extreme option if compression is LZMA
            int32_t compression_option = compression_level;
            if (extreme_compression) {
                if ((sector_tools_compression)streams_script[i].stream_data.compression == C_LZMA) {
                    compression_option |= LZMA_PRESET_EXTREME;
                }
                else if ((sector_tools_compression)streams_script[i].stream_data.compression == C_FLAC) {
                    compression_option |= FLACZLIB_EXTREME_COMPRESSION;
                }
            }
            compobj = new compressor(
                (sector_tools_compression)streams_script[i].stream_data.compression,
                true,
                compression_option
            );

            // Initialize the compressor buffer
            comp_buffer = (uint8_t*) malloc(BUFFER_SIZE);
            if(!comp_buffer) {
                printf("Out of memory\n");
                return ECMTOOL_BUFFER_MEMORY_ERROR;
            }

            // Set the compressor buffer as output
            size_t output_size = BUFFER_SIZE;
            compobj -> set_output(comp_buffer, output_size);
        }

        // Walk through all the sector types in stream
        for (uint32_t j = 0; j < streams_script[i].sectors_data.size(); j++) {
            // Process the number of sectors of every type
            for (uint32_t k = 0; k < streams_script[i].sectors_data[j].sector_count; k++) {
                if (feof(image_in)){
                    printf("Unexpected EOF detected.\n");
                    return ECMTOOL_FILE_READ_ERROR;
                }

                fread(in_sector, 2352, 1, image_in);
                // Compute the crc of the readed data 
                input_edc = sTools->edc_compute(
                    input_edc,
                    in_sector,
                    2352
                );

                // Current sector
                uint32_t current_sector = ftello(image_in) / 2352;

                // We will clean the sector to keep only the data that we want
                uint16_t output_size = 0;
                int8_t res = sTools->clean_sector(
                    out_sector,
                    in_sector,
                    (sector_tools_types)streams_script[i].sectors_data[j].mode,
                    output_size,
                    optimizations
                );

                if (res) {
                    printf("\nThere was an error cleaning the sector\n");
                    return ECMTOOL_PROCESSING_ERROR;
                }

                sectors_type[streams_script[i].sectors_data[j].mode]++;

                // Compress the sector using the selected compression (or none)
                switch (streams_script[i].stream_data.compression) {
                // No compression
                case C_NONE:
                    fwrite(out_sector, output_size, 1, ecm_out);
                    if (ferror(ecm_out)) {
                        printf("\nThere was an error writting the output file");
                        return ECMTOOL_FILE_WRITE_ERROR;
                    }
                    break;

                // Zlib compression
                case C_ZLIB:
                case C_LZMA:
                case C_LZ4:
                case C_FLAC:
                    size_t compress_buffer_left = 0;
                    // Current sector is the last stream sector
                    if (current_sector == streams_script[i].stream_data.end_sector) {
                        res = compobj -> compress(compress_buffer_left, out_sector, output_size, Z_FINISH);
                    }
                    else if (seekable && (sectors_per_block == 1 || !((current_sector + 1) % sectors_per_block))) {
                        // A new compressor block is required
                        res = compobj -> compress(compress_buffer_left, out_sector, output_size, Z_FULL_FLUSH);
                    }
                    else {
                        res = compobj -> compress(compress_buffer_left, out_sector, output_size, Z_NO_FLUSH);
                    }

                    // If buffer is above 75% or is the last sector, write the data to the output and reset the state
                    if (compress_buffer_left < (BUFFER_SIZE * 0.25) || (current_sector) == streams_script[i].stream_data.end_sector) {
                        fwrite(comp_buffer, BUFFER_SIZE - compress_buffer_left, 1, ecm_out);
                        if (ferror(ecm_out)) {
                            printf("\nThere was an error writting the output file");
                            return ECMTOOL_FILE_WRITE_ERROR;
                        }
                        size_t output_size = BUFFER_SIZE;
                        compobj -> set_output(comp_buffer, output_size);
                    }
                    break;
                }

                setcounter_encode(ftello(image_in));
                // If we are not in end of file, sum a sector
                current_sector++;
            }
        }

        if (compobj) {
            delete  compobj;
            compobj = NULL;
        }
        if (comp_buffer) {
            free(comp_buffer);
        }
    
        streams_script[i].stream_data.out_end_position = ftello(ecm_out);
    }

    return ECMTOOL_OK;
}


static ecmtool_return_code disk_decode (
    sector_tools * sTools,
    FILE * ecm_in,
    FILE * image_out,
    std::vector<STREAM_SCRIPT> & streams_script,
    optimization_options optimizations
) {
    // Sectors buffers
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    // Sector counter
    uint32_t current_sector = 0;

    // CRC calculator
    uint32_t original_edc = 0;
    uint32_t output_edc = 0;

    // Stream processing
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        // Compressor object
        compressor *decompobj = NULL;
        // Buffer object
        uint8_t* decomp_buffer = NULL;

        // Initialize the compressor and the buffer if required
        if (streams_script[i].stream_data.compression) {
            // Create the decompression buffer
            decomp_buffer = (uint8_t*) malloc(BUFFER_SIZE);
            if(!decomp_buffer) {
                printf("Out of memory\n");
                return ECMTOOL_BUFFER_MEMORY_ERROR;
            }
            // Check if stream size is smaller than the buffer size and use the smaller size as "to_read"
            size_t to_read = BUFFER_SIZE;
            size_t stream_size = streams_script[i].stream_data.out_end_position - ftello(ecm_in);
            if (to_read > stream_size) {
                to_read = stream_size;
            }
            // Read the data into the buffer
            fread(decomp_buffer, to_read, 1, ecm_in);
            // Create a new decompressor object
            decompobj = new compressor((sector_tools_compression)streams_script[i].stream_data.compression, false);
            // Set the input buffer position as "input" in decompressor object
            decompobj -> set_input(decomp_buffer, to_read);
        }

        // Walk through all the sector types in stream
        for (uint32_t j = 0; j < streams_script[i].sectors_data.size(); j++) {
            // Process the number of sectors of every type
            for (uint32_t k = 0; k < streams_script[i].sectors_data[j].sector_count; k++) {
                if (feof(ecm_in)){
                    printf("Unexpected EOF detected.\n");
                    return ECMTOOL_FILE_READ_ERROR;
                }

                size_t bytes_to_read = 0;
                // Getting the sector size prior to read, to read the real sector size and avoid to fseek every time
                sTools->encoded_sector_size(
                    (sector_tools_types)streams_script[i].sectors_data[j].mode,
                    bytes_to_read,
                    optimizations
                );

                size_t decompress_buffer_left = 0;
                switch (streams_script[i].stream_data.compression) {
                // No compression
                case C_NONE:
                    fread(in_sector, bytes_to_read, 1, ecm_in);
                    break;

                // Zlib/LZMA/LZ4 compression
                case C_ZLIB:
                case C_LZMA:
                case C_LZ4:
                case C_FLAC:
                    // Decompress the sector data
                    decompobj -> decompress(in_sector, bytes_to_read, decompress_buffer_left, Z_SYNC_FLUSH);

                    // If not in end of stream and buffer is below 25%, read more data
                    // To keep the buffer always ready
                    if (streams_script[i].stream_data.out_end_position > ftello(ecm_in) && decompress_buffer_left < (BUFFER_SIZE * 0.25)) {
                        // Move the left data to first bytes
                        size_t position = BUFFER_SIZE - decompress_buffer_left;
                        memmove(decomp_buffer, decomp_buffer + position, decompress_buffer_left);

                        // Calculate how much data can be readed
                        size_t to_read = BUFFER_SIZE - decompress_buffer_left;
                        // If available space is bigger than data in stream, read only the stream data
                        size_t stream_size = streams_script[i].stream_data.out_end_position - ftello(ecm_in);
                        if (to_read > stream_size) {
                            to_read = stream_size;  
                            // If left data is less than buffer space, then end_of_stream is reached
                        }
                        // Fill the buffer with the stream data
                        fread(decomp_buffer + decompress_buffer_left, to_read, 1, ecm_in);
                        // Set again the input position to first byte in decomp_buffer and set the buffer size
                        size_t input_size = decompress_buffer_left + to_read;
                        decompobj -> set_input(decomp_buffer, input_size);
                    }
                }

                // Regenerating the sector data
                uint16_t bytes_readed = 0;
                sTools->regenerate_sector(
                    out_sector,
                    in_sector,
                    (sector_tools_types)streams_script[i].sectors_data[j].mode,
                    current_sector + 0x96, // 0x96 is the first sector "time", equivalent to 00:02:00
                    bytes_readed,
                    optimizations
                );

                // Writting the sector to output file
                fwrite(out_sector, 2352, 1, image_out);
                // Compute the crc of the written data 
                output_edc = sTools->edc_compute(
                    output_edc,
                    out_sector,
                    2352
                );

                // Set the current position in file
                setcounter_decode(ftello(ecm_in));
                current_sector++;
            }
        }

        // Seek to the next stream start position:
        fseeko(ecm_in, streams_script[i].stream_data.out_end_position, SEEK_SET);

        if (decompobj) {
            delete  decompobj;
            decompobj = NULL;
        }
        if (decomp_buffer) {
            free(decomp_buffer);
        }
    }

    // There is no more data in header. Next 4 bytes might be the CRC
    // Reading it...
    uint8_t buffer_edc[4];
    fread(buffer_edc, 4, 1, ecm_in);
    original_edc = sTools->get32lsb(buffer_edc);

    if (original_edc == output_edc) {
        printf("\n\nFinished!\n");
        return ECMTOOL_OK;
    }
    else {
        printf("\n\nWrong CRC!... Maybe the input file is damaged.\n");
        return ECMTOOL_PROCESSING_ERROR;
    }
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


/**
 * @brief Converts an standard streams & sectors streams to a STREAM_SCRIPT vector. Also will check
 *        if they are correct
 * 
 * 
 * @param stream_header The streams header with their data
 * @param stream_header_size The streams count in header
 * @param sectors_header The sectors header whith their data
 * @param sectors_header_size The sectors count in header
 * @param streams_script The output vector which will contains the script
 * @return int 
 */
static ecmtool_return_code task_maker (
    STREAM * streams_toc,
    SEC_STR_SIZE & streams_toc_count,
    SECTOR * sectors_toc,
    SEC_STR_SIZE & sectors_toc_count,
    std::vector<STREAM_SCRIPT> & streams_script
) {
    size_t actual_sector = 0;
    uint32_t actual_sector_pos = 0;

    for (uint32_t i = 0; i <= streams_toc_count.count; i++) {
        streams_script.push_back(STREAM_SCRIPT());
        streams_script.back().stream_data = streams_toc[i];

        while (actual_sector < streams_toc[i].end_sector) {
            if (actual_sector_pos > sectors_toc_count.count) {
                // The streams sectors doesn't fit the sectors count
                // Headers could be corrupted
                return ECMTOOL_CORRUPTED_STREAM;
            }
            // Append the sector data to the current stream
            streams_script.back().sectors_data.push_back(sectors_toc[actual_sector_pos]);

            actual_sector += sectors_toc[actual_sector_pos].sector_count;
            actual_sector_pos++;
        }

        if (actual_sector > streams_toc[i].end_sector) {
            // The actual sector must be equal to last sector in stream, otherwise could be corrupted
            return ECMTOOL_PROCESSING_ERROR;
        }
    }

    return ECMTOOL_OK;
}