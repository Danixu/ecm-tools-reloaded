/*******************************************************************************
 * 
 * Created by Daniel Carrasco at https://www.electrosoftcloud.com
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/

#include "examples_common.h"


static struct option long_options[] = {
    {"input", required_argument, NULL, 'i'},
    {"random", required_argument, NULL, 'r'},
    {"sector", required_argument, NULL, 's'},
    {NULL, 0, NULL, 0}
};



int main(int argc, char **argv) {
    // ECM processor options
    ecm_options options;

    // Input/Output files
    std::ifstream in_file;
    std::fstream toc_file;
    uint32_t in_file_sectors;

    // Clock measurements
    std::chrono::high_resolution_clock::time_point timer_start;
    std::chrono::high_resolution_clock::time_point timer_end;
    std::chrono::microseconds timer_duration;

    // Return code.
    int return_code = 0;

    return_code = get_options(argc, argv, &options);
    if (return_code) {
        goto exit;
    }

    // Open the input file and check if was open correctly
    in_file.open(options.in_filename.c_str(), std::ios::binary);
    {
        char dummy;
        if (!in_file.read(&dummy, 0)) {
            fprintf(stderr, "ERROR: input file cannot be opened.\n");
            return 1;
        }
    }

    // Get the filesize in sectors
    in_file.seekg(0, std::ios_base::end);
    in_file_sectors = in_file.tellg() / 2352;
    in_file.seekg(0, std::ios_base::beg);

    // Image and sector data
    toc_file_header * toc_header;
    toc_file_sector * toc_sector;
    // blocks toc vector
    std::vector<blocks_toc> file_blocks_toc;

    // Check if the file is an ECM2v3 File
    {
        char file_format[5];
        in_file.read(file_format, 4);

        if (
            file_format[0] == 'E' &&
            file_format[1] == 'C' &&
            file_format[2] == 'M'
        ) {
            // File is an ECM2 file, but we need to check the version
            if (file_format[3] == ECM_FILE_VERSION) {
                fprintf(stdout, "An ECM2 file was detected... decoding seek will be used\n");
                options.ecm_file = true;
            }
            else {
                fprintf(stderr, "The input file ECM version is not supported.\n");
                return_code = 1;
                goto exit;
            }
        }
        else {
            fprintf(stdout, "A BIN file was detected... normal seek will be used\n");
        }
    }


    // If random option is set, add the provided number of sectors.
    if (options.random) {
        for (uint32_t i = 0; i < options.random; i++) {
            options.sector_to_read.push_back(0);
            options.sector_to_read.back() = rand() % in_file_sectors;
        }
    }
    

    // Check if any of the provided sectors is out of bounds
    for (uint32_t i = 0; i < options.sector_to_read.size(); i++) {
        if (options.sector_to_read[i] > in_file_sectors) {
            fprintf(stderr, "Sector number %d is bigger than file sectos count (%d)\n", options.sector_to_read[i], in_file_sectors);
            return_code = 1;
            goto exit;
        }
    }

    if (!options.ecm_file) {
        char sector_data[2352];
        // Start the timer to measure execution time
        timer_start = std::chrono::high_resolution_clock::now();

        // Normal seek will be used, to toc file is not required
        for (uint32_t i = 0; i < options.sector_to_read.size(); i++) {
            uint64_t position = options.sector_to_read[i] * 2352;
            in_file.seekg(position, std::ios_base::beg);
            if (!in_file.read(sector_data, 2352)) {
                fprintf(stderr, "ERROR: input file cannot be readed.\n");
                return_code = 1;
                goto exit;
            }
        }

        // End the timer to measure execution time
        timer_end = std::chrono::high_resolution_clock::now();

        fprintf(stdout, "Sector/s were readed...\n");

    }
    else {
        // ECM2 file requires a TOC file to seek faster
        toc_file.open(options.in_filename_toc.c_str(), std::ios::binary|std::ios_base::ate);

        //
        // We will check if TOC file is correct by doing some checks.
        //
        // First, to seek to see if file was just created
        toc_header = new toc_file_header();
        if (toc_file.read((char *)toc_header, sizeof(toc_header))) {
            // To-Do -> Check if file is correct
        }
        else {
            // We will create the toc file

            // Get the image data
            {
                uint64_t toc_position = 0;
                block_header toc_block_header;
                // Will be setted later
                uint64_t ecm_block_start_position;

                // Sectors TOC
                sector *sectors_toc = NULL;
                sec_str_size sectors_toc_header = {C_NONE, 0, 0, 0};
                uint8_t *sectors_toc_c_buffer;

                // Streams TOC
                stream *streams_toc = NULL;
                sec_str_size streams_toc_header = {C_NONE, 0, 0, 0};
                uint8_t *streams_toc_c_buffer;

                in_file.seekg(4, std::ios_base::beg);
                // Read TOC position
                in_file.read(reinterpret_cast<char*>(&toc_position), sizeof(toc_position));

                // Read the TOC block header
                in_file.seekg(toc_position, std::ios_base::beg);
                in_file.read(reinterpret_cast<char*>(&toc_block_header), sizeof(toc_block_header));

                // Read the TOC
                file_blocks_toc.resize(toc_block_header.real_block_size / sizeof(struct blocks_toc));
                in_file.read(reinterpret_cast<char*>(file_blocks_toc.data()), toc_block_header.real_block_size);

                // Locate the image
                for (int i = 0; i < file_blocks_toc.size(); i++) {
                    if (file_blocks_toc[i].type == ECMFILE_BLOCK_TYPE_ECM) {
                        in_file.seekg(file_blocks_toc[i].start_position, std::ios_base::beg);
                        break;
                    }
                }

                // Read block header
                block_header ecm_block_header;
                ecm_header ecm_data_header;
                // Struct size without the strings
                uint32_t ecm_data_header_size = sizeof(ecm_data_header) - sizeof(ecm_data_header.title) - sizeof(ecm_data_header.id);
                // Read the block header
                return_code = read_block_header(in_file, &ecm_block_header);
                if (return_code) {
                    //goto exit;
                }

                // First ECM block byte
                ecm_block_start_position = in_file.tellg();

                // Read the ECM data header
                in_file.read(reinterpret_cast<char*>(&ecm_data_header), ecm_data_header_size);
                if (!in_file.good()) {
                    return_code = 1;
                    goto exit;
                }

                // Read the title stored in file if exists
                if (ecm_data_header.title_length) {
                    ecm_data_header.title.resize(ecm_data_header.title_length);
                    in_file.read((char *)ecm_data_header.title.data(), ecm_data_header.title_length);
                    if (!in_file.good()) {
                        return_code = 1;
                        goto exit;
                    }
                }

                // Read the ID stored in file if exists
                if (ecm_data_header.id_length) {
                    ecm_data_header.id.resize(ecm_data_header.id_length);
                    in_file.read((char *)ecm_data_header.id.data(), ecm_data_header.id_length);
                    if (!in_file.good()) {
                        return_code = 1;
                        goto exit;
                    }
                }

                // Set the optimization options used in file
                toc_header->optimizations = (optimization_options)ecm_data_header.optimizations;

                //
                // Read the streams toc header
                in_file.seekg(ecm_data_header.streams_toc_pos + ecm_block_start_position, std::ios_base::beg);
                in_file.read(reinterpret_cast<char*>(&streams_toc_header), sizeof(streams_toc_header));
                // Read the compressed stream toc data
                streams_toc_c_buffer = (uint8_t *)malloc(streams_toc_header.compressed_size);
                if (!streams_toc_c_buffer) {
                    return_code = 1;
                    goto exit;
                }
                in_file.read(reinterpret_cast<char*>(streams_toc_c_buffer), streams_toc_header.compressed_size);
                if (!in_file.good()) {
                    printf("Error reading the in file: %s\n", strerror(errno));
                    return_code = 1;
                    goto exit;
                }
                // Decompress the streams toc data
                streams_toc = (stream *)malloc(streams_toc_header.uncompressed_size);
                if (decompress_header((uint8_t *)streams_toc, streams_toc_header.uncompressed_size, streams_toc_c_buffer, streams_toc_header.compressed_size)) {
                    fprintf(stderr, "There was an error decompressing the streams header.\n");
                    return_code = ECMTOOL_HEADER_COMPRESSION_ERROR;
                    goto exit;
                }
                free(streams_toc_c_buffer);
                streams_toc_c_buffer = NULL;

                //
                // Read the sectors toc header
                in_file.seekg(ecm_data_header.sectors_toc_pos + ecm_block_start_position, std::ios_base::beg);
                in_file.read(reinterpret_cast<char*>(&sectors_toc_header), sizeof(sectors_toc_header));
                // Read the compressed stream toc data
                sectors_toc_c_buffer = (uint8_t *)malloc(sectors_toc_header.compressed_size);
                if (!sectors_toc_c_buffer) {
                    return_code = 1;
                    goto exit;
                }
                in_file.read(reinterpret_cast<char*>(sectors_toc_c_buffer), sectors_toc_header.compressed_size);
                if (!in_file.good()) {
                    return_code = 1;
                    goto exit;
                }
                // Decompress the strams toc data
                sectors_toc = (sector *)malloc(sectors_toc_header.uncompressed_size);
                if (decompress_header((uint8_t *)sectors_toc, sectors_toc_header.uncompressed_size, sectors_toc_c_buffer, sectors_toc_header.compressed_size)) {
                    fprintf(stderr, "There was an error decompressing the sectors header.\n");
                    return_code = ECMTOOL_HEADER_COMPRESSION_ERROR;
                    goto exit;
                }
                free(sectors_toc_c_buffer);
                sectors_toc_c_buffer = NULL;

                // Now we are at start point
                toc_header->image_position = in_file.tellg();
                toc_header->image_filesize = ecm_block_header.real_block_size - (toc_header->image_position - ecm_block_start_position); // The block size, less the header size

                // Generate the sectors info
                uint64_t relative_position = 0;


                
            }
        }
    }

    timer_duration = std::chrono::duration_cast<std::chrono::microseconds>(timer_end - timer_start);
    fprintf(stdout, "Total time in microseconds: %d\n\n", timer_duration);

    exit:
    if (in_file.is_open()) {
        in_file.close();
    }
}

int read_block_header(
    std::ifstream &in_file,
    block_header *block_header_data
) {
    if (in_file.good()) {
        // Write the header data
        in_file.read(reinterpret_cast<char*>(block_header_data), sizeof(*block_header_data));

        if (in_file.good()) {
            return 0;
        }
        else {
            return 1;
        }
    }
    else {
        fprintf(stderr, "Error reading the header because the input file is not correct\n.");
        return 1;
    }

    return 0;
}

/**
 * @brief Arguments parser for the program. It stores the options in the options struct
 * 
 * @param argc: Number of arguments passed to the program
 * @param argv: Array with the passed arguments
 * @param options: The output struct to place the parsed options
 * @return int: non zero on error
 */
int get_options(
    int argc,
    char **argv,
    ecm_options *options
) {
    char ch;
    // temporal variables for options parsing
    uint64_t temp_argument = 0;

    while ((ch = getopt_long(argc, argv, "i:r:s:?", long_options, NULL)) != -1)
    {
        // check to see if a single character or long option came through
        switch (ch)
        {
            // short option '-i', long option '--input'
            case 'i':
                options->in_filename = optarg;
                options->in_filename_toc = optarg;
                options->in_filename_toc += ".toc"; // Append .toc extension to filename
                break;

            // short option '-s', long option "--keep-output"
            case 'r':
                {
                    std::string optarg_s(optarg);
                    uint32_t temp_argument = std::stoi(optarg_s);
                    options->random = temp_argument;
                    break;
                }

            // short option '-s', long option "--keep-output"
            case 's':
                {
                    std::string optarg_s(optarg);
                    uint64_t temp_argument = std::stoi(optarg_s);
                    options->sector_to_read.push_back(0);
                    options->sector_to_read.back() = temp_argument;
                    break;
                }

            case '?':
                print_help();
                return 0;
                break;
        }
    }

    if (options->in_filename.empty() || (options->sector_to_read.empty() && options->random == 0)) {
        fprintf(stderr, "In file and sectors to read or random, are mandatory.\n\n");
        print_help();
        return 1;
    }

    return 0;
}

int decompress_header (
    uint8_t *dest,
    uint32_t &destLen,
    uint8_t *source,
    uint32_t sourceLen
) {
    z_stream strm;
    int err;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    err = inflateInit(&strm);
    if (err != Z_OK) return err;

    strm.next_out = (uint8_t *)dest;
    strm.avail_out = destLen;
    strm.next_in = (uint8_t *)source;
    strm.avail_in = sourceLen;

    err = inflate(&strm, Z_NO_FLUSH);
    inflateEnd(&strm);

    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && strm.avail_out ? Z_DATA_ERROR :
           err;
}

void print_help() {
    fprintf(stderr, 
        "Usage:\n"
        "\n"
        "To read a sector:\n"
        "    ecmtool -i/--input cdimagefile -s/--sector <sector-number>\n"
        "    ecmtool -i/--input cdimagefile -r/--random <number-of-random-sectors>\n"
        "\n"
    );
}