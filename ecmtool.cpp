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

#include "ecmtool.h"

#define ECM_FILE_VERSION 3

// Some necessary variables
static uint8_t mycounter_analyze = (off_t)-1;
static uint8_t mycounter_encode  = (off_t)-1;
static off_t mycounter_decode  = (off_t)-1;
static off_t mycounter_total   = 0;

static struct option long_options[] = {
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {"acompression", required_argument, NULL, 'a'},
    {"dcompression", required_argument, NULL, 'd'},
    {"clevel", required_argument, NULL, 'c'},
    {"extreme-compression", no_argument, NULL, 'e'},
    {"seekable", no_argument, NULL, 's'},
    {"sectors-per-block", required_argument, NULL, 'p'},
    {"force", required_argument, NULL, 'f'},
    {"keep-output", required_argument, NULL, 'k'},
    {NULL, 0, NULL, 0}
};


int main(int argc, char **argv) {
    std::ios_base::sync_with_stdio(false);
    // ECM processor options
    ecm_options options;

    // Input file will be decoded
    bool decode = false;

    // Input/Output files
    std::ifstream in_file;
    std::fstream out_file;

    // blocks toc vector
    std::vector<blocks_toc> file_blocks_toc; 

    // Start the timer to measure execution time
    auto start = std::chrono::high_resolution_clock::now();

    // Return code.
    int return_code = 0;

    return_code = get_options(argc, argv, &options);
    if (return_code) {
        goto exit;
    }

    if (options.in_filename.empty()) {
        fprintf(stderr, "ERROR: input file is required.\n");
        print_help();
        return 1;
    }

    // Open the input file
    in_file.open(options.in_filename.c_str(), std::ios::binary);
    // Tricky way to check if was oppened correctly.
    // The "is_open" method was failing on cross compiled EXE
    {
        char dummy;
        if (!in_file.read(&dummy, 0)) {
            fprintf(stderr, "ERROR: input file cannot be opened.\n");
            return 1;
        }
    }

    // Check if the file is an ECM3 File
    {
        char file_format[5];
        in_file.read(file_format, 4);

        if (
            file_format[0] == 'E' &&
            file_format[1] == 'C' &&
            file_format[2] == 'M'
        ) {
            // File is an ECM file, but we need to check the version
            if (file_format[3] == ECM_FILE_VERSION) {
                fprintf(stdout, "An ECM file was detected... will be decoded\n");
                decode = true;
            }
            else {
                fprintf(stderr, "The input file ECM version is not supported.\n");
                return_code = 1;
                goto exit;
            }
        }
        else {
            fprintf(stdout, "A BIN file was detected... will be encoded\n");
        }
    }

    // If no output filename was provided, generate it using the input filename
    if (options.out_filename.empty()) {
        // Input file will be decoded, so ecm2 extension must be removed (if exists)
        if (decode) {
            // Copy the original string into a tmp string to force the same length
            std::string tmp_filename = options.in_filename;
            // Convert it to lowercase to easily check file extension
            std::transform(options.in_filename.begin(), options.in_filename.end(), tmp_filename.begin(), ::tolower);
            if (tmp_filename.substr(tmp_filename.length() - 5) == std::string(".ecm2")) {
                options.out_filename = options.in_filename.substr(0, options.in_filename.length() - 5);
            }
            // If there is no .ecm2 extension, then .unecm2 will be appended to avoid to use the same filename
            else {
                options.out_filename = options.in_filename + ".unecm2";
            }
        }
        else {
            options.out_filename = options.in_filename + ".ecm2";
        }
    }

    // Check if output file exists only if force_rewrite is false
    if (options.force_rewrite == false) {
        char dummy;
        out_file.open(options.out_filename.c_str(), std::ios::in|std::ios::binary);
        if (out_file.read(&dummy, 0)) {
            fprintf(stderr, "ERROR: Cowardly refusing to replace output file. Use the -f/--force-rewrite options to force it.\n");
            options.keep_output = true;
            return_code = 1;
            goto exit;
        }
        out_file.close();
    }

    // Open the output file in replace mode
    out_file.open(options.out_filename.c_str(), std::ios::out|std::ios::binary);
    // Check if file was oppened correctly.
    if (!out_file.good()) {
        fprintf(stderr, "ERROR: output file cannot be opened.\n");
        return_code = 1;
        goto exit;
    }

    // Encoding process
    if (!decode) {
        // Set output ECM header
        out_file << "ECM" << char(ECM_FILE_VERSION);
        // Dummy TOC position
        uint64_t toc_position = 0;
        out_file.write(reinterpret_cast<char*>(&toc_position), sizeof(toc_position));

        // File TOC header
        block_header toc_block_header = {ECMFILE_BLOCK_TYPE_TOC, 0, 0, 0};

        // Add the ECM data TOC
        file_blocks_toc.push_back(blocks_toc());
        file_blocks_toc.back().type = ECMFILE_BLOCK_TYPE_ECM;
        file_blocks_toc.back().start_position = out_file.tellp();

        std::vector<uint32_t> sectors_type_sumary;
        sectors_type_sumary.resize(13);
        return_code = image_to_ecm_block(in_file, out_file, &options, &sectors_type_sumary);
        if (return_code) {
            fprintf(stderr, "\nERROR: there was an error processing the input file.\n");
            return_code = 1;
            goto exit;
        }

        // Write the Table of content
        toc_position = out_file.tellp();
        toc_block_header.real_block_size = file_blocks_toc.size() * sizeof(struct blocks_toc);
        toc_block_header.block_size = toc_block_header.real_block_size;
        // Write the Table of content header
        out_file.write(reinterpret_cast<char*>(&toc_block_header), sizeof(toc_block_header));
        // Write the Table of content data
        out_file.write(reinterpret_cast<char*>(file_blocks_toc.data()), toc_block_header.block_size);
        // Rewrite the Table of content position
        out_file.seekp(4);
        out_file.write(reinterpret_cast<char*>(&toc_position), sizeof(toc_position));
    }
    // Decoding process
    else {
        // Variables
        uint64_t toc_position = 0;
        block_header toc_block_header;

        in_file.seekg(4, std::ios_base::beg);
        // Read TOC position
        in_file.read(reinterpret_cast<char*>(&toc_position), sizeof(toc_position));

        // Read the TOC block header
        in_file.seekg(toc_position, std::ios_base::beg);
        in_file.read(reinterpret_cast<char*>(&toc_block_header), sizeof(toc_block_header));

        // Read the TOC
        file_blocks_toc.resize(toc_block_header.real_block_size / sizeof(struct blocks_toc));
        in_file.read(reinterpret_cast<char*>(file_blocks_toc.data()), toc_block_header.real_block_size);

        for (int i = 0; i < file_blocks_toc.size(); i++) {
            if (file_blocks_toc[i].type == ECMFILE_BLOCK_TYPE_ECM) {
                in_file.seekg(file_blocks_toc[i].start_position, std::ios_base::beg);
                return_code = ecm_block_to_image(in_file, out_file, &options);
            }
        }
    }

    exit:
    if (in_file.is_open()) {
        in_file.close();
    }
    if (out_file.is_open()) {
        out_file.close();
    }

    if (return_code == 0) {
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start);
        printf("\n\nTotal execution time: %0.3fs\n\n", duration.count() / 1000.0F);
    }
    else {
        if (!options.keep_output) {
            // Something went wrong, so output file must be deleted if keep == false
            // We will remove the file if something went wrong
            std::ifstream out_remove_tmp(options.out_filename.c_str(), std::ios::binary);
            char dummy;
            if (out_remove_tmp.read(&dummy, 0)) {
                out_remove_tmp.close();
                if (remove(options.out_filename.c_str())) {
                    fprintf(stderr, "There was an error removing the output file... Please remove it manually.\n");
                }
            }
        }
    }
    return return_code;
}


int image_to_ecm_block(
    std::ifstream &in_file,
    std::fstream &out_file,
    ecm_options *options,
    std::vector<uint32_t> *sectors_type_sumary
) {
    // Input size
    in_file.seekg(0, std::ios_base::end);
    size_t in_total_size = in_file.tellg();
    in_file.seekg(0, std::ios_base::beg);

    // Stream script to the encode process
    std::vector<stream_script> streams_script;

    // Sectors TOC
    sector *sectors_toc = NULL;
    sec_str_size sectors_toc_header = {C_NONE, 0, 0, 0};
    uint8_t *sectors_toc_c_buffer;

    // Streams TOC
    stream *streams_toc = NULL;
    sec_str_size streams_toc_header = {C_NONE, 0, 0, 0};
    uint8_t *streams_toc_c_buffer;

    // Sector Tools object
    sector_tools *sTools;

    // Return code
    int return_code = 0;

    if (in_total_size % 2352) {
        fprintf(stderr, "ERROR: The input file doesn't appear to be a CD-ROM image\n");
        fprintf(stderr, "       This program only allows to process CD-ROM images\n");

        return ECMTOOL_FILE_READ_ERROR;
    }

    // Reset the counters
    resetcounter(in_total_size);

    // Sector Tools object
    sTools = new sector_tools();
    // Block header with type 2 (ECM data), no compression, and no size for now
    // Later will be completed when data is known
    block_header ecm_block_header = {2, 0, 0, 0};
    // ECM Header
    ecm_header ecm_data_header = {
        options->optimizations,
        options->seekable ? options->sectors_per_block : (uint8_t)0,
        0,
        0,
        0,
        0,
        0,
        0,
        "",
        ""
    };
    // Struct size without the strings
    uint32_t ecm_data_header_size = sizeof(ecm_data_header) - sizeof(ecm_data_header.title) - sizeof(ecm_data_header.id);

    // First blocks byte
    uint64_t block_start_position = out_file.tellp();
    // Will be setted later
    uint64_t ecm_block_start_position = 0;

    // Write the "dummy" block header
    return_code = write_block_header(out_file, &ecm_block_header);
    if (return_code) {
        goto exit;
    }

    // First ECM block byte
    ecm_block_start_position = out_file.tellp();

    // Write the ECM dummy header
    out_file.write(reinterpret_cast<char*>(&ecm_data_header), ecm_data_header_size);
    if (!out_file.good()) {
        return_code = 1;
        goto exit;
    }
    out_file << ecm_data_header.title;
    out_file << ecm_data_header.id;

    // Analyze the disk to detect the sectors types
    return_code = disk_analyzer (
        sTools,
        in_file,
        in_total_size,
        streams_script,
        &ecm_data_header,
        options
    );

    //
    // Time to write the streams header
    //
    ecm_data_header.streams_toc_pos = (uint64_t)out_file.tellp() - ecm_block_start_position;
    return_code = task_to_streams_header (
        streams_toc,
        streams_toc_header,
        streams_script
    );
    if (return_code) {
        goto exit;
    }
    // Compress the streams header. Sectors count is base 0, so one will be added to the size calculation
    {
        // Compressed size will be the uncompressed size + 6 zlib header bytes + 5 zlib block headers for every 16k (plus two extra for security)
        uint32_t compressed_size = streams_toc_header.uncompressed_size + 6 + (((streams_toc_header.uncompressed_size / 16.384) + 3) * 5);
        streams_toc_c_buffer = (uint8_t *)malloc(compressed_size);
        if(!streams_toc_c_buffer) {
            fprintf(stderr, "Out of memory\n");
            return_code = ECMTOOL_BUFFER_MEMORY_ERROR;
            goto exit;
        }
        if (compress_header(streams_toc_c_buffer, compressed_size, (uint8_t *)streams_toc, streams_toc_header.uncompressed_size, 9)) {
            fprintf(stderr, "There was an error compressing the streams header.\n");
            return_code = ECMTOOL_HEADER_COMPRESSION_ERROR;
            goto exit;
        }
        streams_toc_header.compressed_size = compressed_size;
        streams_toc_header.compression = C_ZLIB;
    }
    // Write the compressed header
    out_file.write(reinterpret_cast<char*>(&streams_toc_header), sizeof(streams_toc_header));
    out_file.write(reinterpret_cast<char*>(streams_toc_c_buffer), streams_toc_header.compressed_size);
    // Free the header memory
    free(streams_toc);
    streams_toc = NULL;
    free(streams_toc_c_buffer);
    streams_toc_c_buffer = NULL;

    //
    // Time to write the sectors header
    //
    ecm_data_header.sectors_toc_pos = (uint64_t)out_file.tellp() - ecm_block_start_position;
    return_code = task_to_sectors_header (
        sectors_toc,
        sectors_toc_header,
        streams_script
    );
    if (return_code) {
        goto exit;
    }
    // Compress the sectors header.  Sectors count is base 0, so one will be added to the size calculation
    {
        // Compressed size will be the uncompressed size + 6 zlib header bytes + 5 zlib block headers for every 16k (plus two extra for security)
        uint32_t compressed_size = sectors_toc_header.uncompressed_size + 6 + (((sectors_toc_header.uncompressed_size / 16.384) + 3) * 5);
        sectors_toc_c_buffer = (uint8_t*) malloc(compressed_size);
        if(!sectors_toc_c_buffer) {
            fprintf(stderr, "Out of memory\n");
            return_code = ECMTOOL_BUFFER_MEMORY_ERROR;
            goto exit;
        }
        if (compress_header(sectors_toc_c_buffer, compressed_size, (uint8_t *)sectors_toc, sectors_toc_header.uncompressed_size, 9)) {
            fprintf(stderr, "There was an error compressing the output sectors header.\n");
            return_code = ECMTOOL_HEADER_COMPRESSION_ERROR;
            goto exit;
        }
        sectors_toc_header.compressed_size = compressed_size;
        sectors_toc_header.compression = C_ZLIB;
    }
    // Write the compressed header
    out_file.write(reinterpret_cast<char*>(&sectors_toc_header), sizeof(sectors_toc_header));
    if (!out_file.good()) {
        return_code = 1;
        goto exit;
    }
    out_file.write(reinterpret_cast<char*>(sectors_toc_c_buffer), sectors_toc_header.compressed_size);
    if (!out_file.good()) {
        return_code = 1;
        goto exit;
    }
    // Free the header memory
    free(sectors_toc);
    sectors_toc = NULL;
    free(sectors_toc_c_buffer);
    sectors_toc_c_buffer = NULL;

    //
    // Now we will write the ECM data header
    //
    ecm_data_header.ecm_data_pos = (uint64_t)out_file.tellp() - ecm_block_start_position;

    //
    // Convert the image to ECM data
    //
    return_code = disk_encode (
        sTools,
        in_file,
        out_file,
        streams_script,
        options,
        sectors_type_sumary
    );
    if (return_code) {
        goto exit;
    }

    // Set the block sizes. Both are equal because this block will not use compression
    ecm_block_header.real_block_size = (uint64_t)out_file.tellp() - ecm_block_start_position;
    ecm_block_header.block_size = (uint64_t)out_file.tellp() - ecm_block_start_position;

    // Write the new block heeader
    out_file.seekp(block_start_position);
    out_file.write(reinterpret_cast<char*>(&ecm_block_header), sizeof(ecm_block_header));
    if (!out_file.good()) {
        return_code = 1;
        goto exit;
    }

    // Finally, write the ecm data block header
    out_file.write(reinterpret_cast<char*>(&ecm_data_header), ecm_data_header_size);
    if (!out_file.good()) {
        return_code = 1;
        goto exit;
    }
    out_file.seekp(0, std::ios_base::end);

    exit:
    // Free the reserved memory for objects
    if (sTools) {
        delete sTools;
    }
    if (streams_toc) {
        delete [] streams_toc;
    }
    if (streams_toc_c_buffer) {
        delete [] streams_toc_c_buffer;
    }
    if (sectors_toc) {
        delete [] sectors_toc;
    }
    if (sectors_toc_c_buffer) {
        delete [] sectors_toc_c_buffer;
    }

    if (!return_code) {
        summary(sectors_type_sumary, options, sTools, (uint64_t)out_file.tellp());
    }

    // Close the files
    if (in_file.is_open()){
        in_file.close();
    }
    if (out_file.is_open()){
        out_file.close();
    }

    return return_code;
}

int ecm_block_to_image(
    std::ifstream &in_file,
    std::fstream &out_file,
    ecm_options *options
) {
    // CRC calculation to check the decoded stream
    uint32_t output_edc = 0;

    // Stream script to the encode process
    std::vector<stream_script> streams_script;

    // ECM headers
    block_header ecm_block_header;
    ecm_header ecm_data_header;
    // Struct size without the strings
    uint32_t ecm_data_header_size = sizeof(ecm_data_header) - sizeof(ecm_data_header.title) - sizeof(ecm_data_header.id);

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

    // Sector Tools object
    sector_tools *sTools = new sector_tools();

    // Return code
    int return_code = 0;

    // Read the block header
    return_code = read_block_header(in_file, &ecm_block_header);
    if (return_code) {
        goto exit;
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
    options->optimizations = (optimization_options)ecm_data_header.optimizations;

    // Reset the counters
    resetcounter(ecm_block_header.block_size);

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

    // Convert the headers to an script to be followed
    return_code = task_maker (
        streams_toc,
        streams_toc_header,
        sectors_toc,
        sectors_toc_header,
        streams_script
    );
    if (return_code) {
        return_code = ECMTOOL_CORRUPTED_STREAM;
        goto exit;
    }

    in_file.seekg(ecm_data_header.ecm_data_pos + ecm_block_start_position, std::ios_base::beg);
    return_code = disk_decode (
        sTools,
        in_file,
        out_file,
        streams_script,
        options,
        ecm_block_start_position
    );

    exit:
    if (sTools) {
        delete sTools;
    }
    if (streams_toc) {
        delete [] streams_toc;
    }
    if (streams_toc_c_buffer) {
        delete [] streams_toc_c_buffer;
    }
    if (sectors_toc) {
        delete [] sectors_toc;
    }
    if (sectors_toc_c_buffer) {
        delete [] sectors_toc_c_buffer;
    }

    return return_code;
}


int write_block_header(
    std::fstream &out_file,
    block_header *block_header_data
) {
    if (out_file.good()) {
        // Write the header data
        out_file.write(reinterpret_cast<char*>(block_header_data), sizeof(*block_header_data));

        if (out_file.good()) {
            return 0;
        }
        else {
            return 1;
        }
    }
    else {
        fprintf(stderr, "Error writting the header because the output file is not correct\n.");
        return 1;
    }

    return 0;
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


static ecmtool_return_code disk_analyzer (
    sector_tools *sTools,
    std::ifstream &in_file,
    size_t image_file_size,
    std::vector<stream_script> &streams_script,
    ecm_header *ecm_data_header,
    ecm_options *options
) {
    // Sector count
    size_t sectors_count = image_file_size / 2352;

    // Sector buffer
    uint8_t in_sector[2352];

    // Sector counter
    uint32_t current_sector = 0;

    // Seek to the begin
    in_file.seekg(0, std::ios_base::beg);

    // Loop through all the sectors
    for (size_t i = 0; i < sectors_count; i++) {
        // Read a sector
        in_file.read(reinterpret_cast<char*>(in_sector), 2352);
        if (in_file.good()) {
            // Update the input file position
            setcounter_analyze(in_file.tellg());

            sector_tools_types detected_type = sTools->detect(in_sector);
            //printf("Current sector: %d -> Type: %d\n", current_sector, detected_type);

            //printf("Detected sector type: %d\n", detected_type);

            // Only check the sector info in data sectors
            if (detected_type != STT_CDDA && detected_type != STT_CDDA_GAP) {
                // Check if MSF can be regenerated (libcrypt protection)
                uint8_t time_data[3];
                sTools->sector_to_time(time_data, current_sector + 0x96);
                if (
                    ecm_data_header->optimizations & OO_REMOVE_MSF &&
                    (time_data[0] != in_sector[0x0C] ||
                    time_data[1] != in_sector[0x0D] ||
                    time_data[2] != in_sector[0x0E])
                ) {
                    // Sector contains wrong MSF, so it cannot be recovered in a lossless way
                    // To avoid this problem, we will disable the sector MSF optimization
                    ecm_data_header->optimizations = (optimization_options)(ecm_data_header->optimizations & ~OO_REMOVE_MSF);
                    options->optimizations = (optimization_options)ecm_data_header->optimizations;
                    //printf("Disabled MSF optimization. current_sector: %d, %d\n\n", current_sector, ecm_data_header->optimizations);
                }

                // Check if redundant FLAG can be regenerated (Only Mode 2 XA modes)
                if (
                    (
                        detected_type == STT_MODE2_1 ||
                        detected_type == STT_MODE2_1_GAP ||
                        detected_type == STT_MODE2_2 ||
                        detected_type == STT_MODE2_2_GAP
                    ) &&
                    ecm_data_header->optimizations & OO_REMOVE_REDUNDANT_FLAG &&
                    (
                        in_sector[0x10] != in_sector[0x14] ||
                        in_sector[0x11] != in_sector[0x15] ||
                        in_sector[0x12] != in_sector[0x16] ||
                        in_sector[0x13] != in_sector[0x17]
                    )
                ) {
                    // Sector contains wrong subheader redundancy, so it cannot be recovered in a lossless way
                    // To avoid this problem, we will disable the sector redundant FLAG removal
                    //printf("Disabled redundant FLAG. current_sector: %d\n\n", current_sector);
                    ecm_data_header->optimizations = (optimization_options)(ecm_data_header->optimizations & ~OO_REMOVE_REDUNDANT_FLAG);
                }
            }

            sector_tools_stream_types stream_type = sTools->detect_stream(detected_type);
            // If there are no streams or stream type is different, create a new streams entry.
            if (
                streams_script.size() == 0 ||
                streams_script.back().stream_data.type != (stream_type - 1)
            ) {
                // Push the new element to the end
                streams_script.push_back(stream_script());

                // Set the element data
                streams_script.back().stream_data.type = stream_type - 1;
                if (stream_type == STST_AUDIO) {
                    streams_script.back().stream_data.compression = options->audio_compression;
                }
                else {
                    streams_script.back().stream_data.compression = options->data_compression;
                }

                if (streams_script.size() > 1) {
                    streams_script.back().stream_data.end_sector = streams_script[streams_script.size() - 2].stream_data.end_sector;
                }
                else {
                    streams_script.back().stream_data.end_sector = 0;
                }
            }

            if (
                streams_script.back().sectors_data.size() == 0 ||
                streams_script.back().sectors_data.back().mode != detected_type
            ) {
                // Push the new element to the end
                streams_script.back().sectors_data.push_back(sector());

                // Set the element data
                streams_script.back().sectors_data.back().mode = detected_type;
                streams_script.back().sectors_data.back().sector_count = 0;
            }

            streams_script.back().stream_data.end_sector++;
            streams_script.back().sectors_data.back().sector_count++;
        }
        else {
            // There was an eror reading the new sector
            fprintf(stderr, "There was an error reading the input file.\n");
            return ECMTOOL_FILE_READ_ERROR;
        }

        current_sector++;
    }

    return ECMTOOL_OK;
}


static ecmtool_return_code disk_encode (
    sector_tools *sTools,
    std::ifstream &in_file,
    std::fstream &out_file,
    std::vector<stream_script> &streams_script,
    ecm_options *options,
    std::vector<uint32_t> *sectors_type
) {
    // Sectors buffers
    uint8_t in_sector[2352];
    uint8_t out_sector[2352];

    // Hash
    uint32_t input_edc = 0;
    uint8_t buffer_edc[4];

    // Reference to sectors_type
    std::vector<uint32_t>& sectors_type_ref = *sectors_type;

    // Seek to the begin
    in_file.seekg(0, std::ios_base::beg);

    // Stream processing
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        // Compressor object
        compressor *compobj = NULL;
        // Buffer object
        uint8_t *comp_buffer = NULL;

        // Initialize the compressor and the buffer if required
        if (streams_script[i].stream_data.compression) {
            // Set compression level with extreme option if compression is LZMA
            int32_t compression_option = options->compression_level;
            if (options->extreme_compression) {
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
                fprintf(stderr, "Out of memory\n");
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
                if (in_file.eof()){
                    fprintf(stderr, "Unexpected EOF detected.\n");
                    return ECMTOOL_FILE_READ_ERROR;
                }

                in_file.read(reinterpret_cast<char*>(in_sector), 2352);
                // Compute the crc of the readed data 
                input_edc = sTools->edc_compute(
                    input_edc,
                    in_sector,
                    2352
                );

                // Current sector
                uint32_t current_sector = in_file.tellg() / 2352;

                // We will clean the sector to keep only the data that we want
                uint16_t output_size = 0;
                int8_t res = sTools->clean_sector(
                    out_sector,
                    in_sector,
                    (sector_tools_types)streams_script[i].sectors_data[j].mode,
                    output_size,
                    options->optimizations
                );

                if (res) {
                    fprintf(stderr, "There was an error cleaning the sector\n");
                    return ECMTOOL_PROCESSING_ERROR;
                }

                sectors_type_ref[streams_script[i].sectors_data[j].mode]++;

                // Compress the sector using the selected compression (or none)
                switch (streams_script[i].stream_data.compression) {
                // No compression
                case C_NONE:
                    out_file.write(reinterpret_cast<char*>(out_sector), output_size);
                    if (!out_file.good()) {
                        fprintf(stderr, "\nThere was an error writting the output file");
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
                    else if (options->seekable && (options->sectors_per_block == 1 || !((current_sector + 1) % options->sectors_per_block))) {
                        // A new compressor block is required
                        res = compobj -> compress(compress_buffer_left, out_sector, output_size, Z_FULL_FLUSH);
                    }
                    else {
                        res = compobj -> compress(compress_buffer_left, out_sector, output_size, Z_NO_FLUSH);
                    }

                    if (res != 0) {
                        fprintf(stderr, "There was an error compressing the stream: %d.\n", res);
                        return ECMTOOL_PROCESSING_ERROR;
                    }

                    // If buffer is above 75% or is the last sector, write the data to the output and reset the state
                    if (compress_buffer_left < (BUFFER_SIZE * 0.25) || (current_sector) == streams_script[i].stream_data.end_sector) {
                        out_file.write(reinterpret_cast<char*>(comp_buffer), BUFFER_SIZE - compress_buffer_left);
                        if (!out_file.good()) {
                            fprintf(stderr, "\nThere was an error writting the output file");
                            return ECMTOOL_FILE_WRITE_ERROR;
                        }
                        size_t output_size = BUFFER_SIZE;
                        compobj -> set_output(comp_buffer, output_size);
                    }
                    break;
                }

                setcounter_encode(in_file.tellg());
            }
        }

        if (compobj) {
            delete  compobj;
            compobj = NULL;
        }
        if (comp_buffer) {
            free(comp_buffer);
        }
    
        streams_script[i].stream_data.out_end_position = out_file.tellp();
    }

    // Write the CRC
    sTools->put32lsb(buffer_edc, input_edc);
    out_file.write(reinterpret_cast<char*>(buffer_edc), 4);

    return ECMTOOL_OK;
}


static ecmtool_return_code disk_decode (
    sector_tools *sTools,
    std::ifstream &in_file,
    std::fstream &out_file,
    std::vector<stream_script> &streams_script,
    ecm_options *options,
    uint64_t ecm_block_start_position
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
        uint8_t *decomp_buffer = NULL;

        // Initialize the compressor and the buffer if required
        if (streams_script[i].stream_data.compression) {
            // Create the decompression buffer
            decomp_buffer = (uint8_t*) malloc(BUFFER_SIZE);
            if(!decomp_buffer) {
                fprintf(stderr, "Out of memory\n");
                return ECMTOOL_BUFFER_MEMORY_ERROR;
            }
            // Check if stream size is smaller than the buffer size and use the smaller size as "to_read"
            size_t to_read = BUFFER_SIZE;
            size_t stream_size = streams_script[i].stream_data.out_end_position - (uint64_t)in_file.tellg();
            if (to_read > stream_size) {
                to_read = stream_size;
            }
            // Read the data into the buffer
            in_file.read(reinterpret_cast<char*>(decomp_buffer), to_read);
            // Create a new decompressor object
            decompobj = new compressor((sector_tools_compression)streams_script[i].stream_data.compression, false);
            // Set the input buffer position as "input" in decompressor object
            decompobj -> set_input(decomp_buffer, to_read);
        }

        // Walk through all the sector types in stream
        for (uint32_t j = 0; j < streams_script[i].sectors_data.size(); j++) {
            // Process the number of sectors of every type
            for (uint32_t k = 0; k < streams_script[i].sectors_data[j].sector_count; k++) {
                if (in_file.eof()){
                    fprintf(stderr, "Unexpected EOF detected.\n");
                    return ECMTOOL_FILE_READ_ERROR;
                }

                size_t bytes_to_read = 0;
                // Getting the sector size prior to read, to read the real sector size and avoid to fseek every time
                sTools->encoded_sector_size(
                    (sector_tools_types)streams_script[i].sectors_data[j].mode,
                    bytes_to_read,
                    options->optimizations
                );

                size_t decompress_buffer_left = 0;
                switch (streams_script[i].stream_data.compression) {
                // No compression
                case C_NONE:
                    in_file.read(reinterpret_cast<char*>(in_sector), bytes_to_read);
                    setcounter_decode((uint64_t)in_file.tellg() - ecm_block_start_position);
                    break;

                // Zlib/LZMA/LZ4 compression
                case C_ZLIB:
                case C_LZMA:
                case C_LZ4:
                case C_FLAC:
                    // Decompress the sector data
                    decompobj -> decompress(in_sector, bytes_to_read, decompress_buffer_left, Z_SYNC_FLUSH);

                    // Set the current position in file
                    setcounter_decode((uint64_t)in_file.tellg() - decompress_buffer_left - ecm_block_start_position); 

                    // If not in end of stream and buffer is below 25%, read more data
                    // To keep the buffer always ready
                    if (streams_script[i].stream_data.out_end_position > (uint64_t)in_file.tellg() && decompress_buffer_left < (BUFFER_SIZE * 0.25)) {
                        // Move the left data to first bytes
                        size_t position = BUFFER_SIZE - decompress_buffer_left;
                        memmove(decomp_buffer, decomp_buffer + position, decompress_buffer_left);

                        // Calculate how much data can be readed
                        size_t to_read = BUFFER_SIZE - decompress_buffer_left;
                        // If available space is bigger than data in stream, read only the stream data
                        size_t stream_size = streams_script[i].stream_data.out_end_position - (uint64_t)in_file.tellg();
                        if (to_read > stream_size) {
                            to_read = stream_size;  
                            // If left data is less than buffer space, then end_of_stream is reached
                        }
                        // Fill the buffer with the stream data
                        in_file.read(reinterpret_cast<char*>(decomp_buffer + decompress_buffer_left), to_read);
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
                    options->optimizations
                );

                // Writting the sector to output file
                out_file.write(reinterpret_cast<char*>(out_sector), 2352);
                // Compute the crc of the written data 
                output_edc = sTools->edc_compute(
                    output_edc,
                    out_sector,
                    2352
                );

                current_sector++;
            }
        }

        // Seek to the next stream start position:
        //fseeko(ecm_in, streams_script[i].stream_data.out_end_position, SEEK_SET);

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
    in_file.read(reinterpret_cast<char*>(buffer_edc), 4);
    original_edc = sTools->get32lsb(buffer_edc);

    if (original_edc == output_edc) {
        return ECMTOOL_OK;
    }
    else {
        fprintf(stderr, "\n\nWrong CRC!... Maybe the input file is damaged.\n");
        return ECMTOOL_PROCESSING_ERROR;
    }
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

    while ((ch = getopt_long(argc, argv, "i:o:a:d:c:esp:fk", long_options, NULL)) != -1)
    {
        // check to see if a single character or long option came through
        switch (ch)
        {
            // short option '-i', long option '--input'
            case 'i':
                options->in_filename = optarg;
                break;

            // short option '-o', long option "--output"
            case 'o':
                options->out_filename = optarg;
                break;

            // short option '-a', long option "--acompression"
            // Audio compression option
            case 'a':
                if (strcmp("zlib", optarg) == 0) {
                    options->audio_compression = C_ZLIB;
                }
                else if (strcmp("lzma", optarg) == 0) {
                    options->audio_compression = C_LZMA;
                }
                else if (strcmp("lz4", optarg) == 0) {
                    options->audio_compression = C_LZ4;
                }
                else if (strcmp("flac", optarg) == 0) {
                    options->audio_compression = C_FLAC;
                }
                else {
                    fprintf(stderr, "ERROR: Unknown data compression mode: %s\n\n", optarg);
                    print_help();
                    return 1;
                }
                break;

            // short option '-d', long option '--dcompression'
            // Data compression option
            case 'd':
                if (strcmp("zlib", optarg) == 0) {
                    options->data_compression = C_ZLIB;
                }
                else if (strcmp("lzma", optarg) == 0) {
                    options->data_compression = C_LZMA;
                }
                else if (strcmp("lz4", optarg) == 0) {
                    options->data_compression = C_LZ4;
                }
                else {
                    fprintf(stderr, "ERROR: Unknown data compression mode: %s\n\n", optarg);
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
                        fprintf(stderr, "ERROR: the provided compression level option is not correct.\n\n");
                        print_help();
                        return 1;
                    }
                    else {
                        options->compression_level = (uint8_t)temp_argument;
                    }
                } catch (std::exception const &e) {
                    fprintf(stderr, "ERROR: the provided compression level option is not correct.\n\n");
                    print_help();
                    return 1;
                }

                break;

            // short option '-e', long option "--extreme-compresison" (only LZMA)
            case 'e':
                options->extreme_compression = true;
                break;

             // short option '-s', long option "--seekable"
            case 's':
                options->seekable = true;
                break;

            // short option '-p', long option "--sectors_per_block"
            case 'p':
                try {
                    std::string optarg_s(optarg);
                    temp_argument = std::stoi(optarg_s);

                    if (!temp_argument || temp_argument > 255 || temp_argument < 0) {
                        fprintf(stderr, "ERROR: the provided sectors per block number is not correct.\n\n");
                        print_help();
                        return 1;
                    }
                    else {
                        options->sectors_per_block = (uint8_t)temp_argument;
                    }
                } catch (std::exception const &e) {
                    fprintf(stderr, "ERROR: the provided sectors per block number is not correct.\n\n");
                    print_help();
                    return 1;
                }
                break;

            // short option '-f', long option "--force"
            case 'f':
                options->force_rewrite = true;
                break;

            // short option '-k', long option "--keep-output"
            case 'k':
                options->keep_output = true;
                break;

            case '?':
                print_help();
                return 0;
                break;
        }
    }

    return 0;
}


static void resetcounter(off_t total) {
    mycounter_analyze = 0;
    mycounter_encode  = 0;
    mycounter_decode  = 0;
    mycounter_total   = total;
}


static void encode_progress(void) {
    /*
    fprintf(stderr,
        "Analyze(%02u%%) Encode(%02u%%)\r",
        mycounter_analyze,
        mycounter_encode
    );
    */
    std::cerr << "Analyze(" << std::setfill('0') << std::setw(2) << unsigned(mycounter_analyze) << "%) Encode(" << std::setfill('0') << std::setw(2) << unsigned(mycounter_encode) << "%)\r";
}


static void decode_progress(void) {
    fprintf(stderr,
        "Decode(%02u%%)\r",
        mycounter_decode
    );
}


static void setcounter_analyze(off_t n) {
    uint8_t p = 100 * n / mycounter_total;
    if (p != mycounter_analyze) {
        mycounter_analyze = p;
        encode_progress();
    }
}


static void setcounter_encode(off_t n) {
    uint8_t p = 100 * n / mycounter_total;
    if (p != mycounter_encode) {
        mycounter_encode = p;
        encode_progress();
    }
}


static void setcounter_decode(off_t n) {
    uint8_t p = 100 * n / mycounter_total;
    if (p != mycounter_decode) {
        mycounter_decode = p;
        decode_progress();
    }
}


static ecmtool_return_code task_to_streams_header (
    stream *&streams_toc,
    sec_str_size &streams_toc_count,
    std::vector<stream_script> &streams_script
) {
    // Set the sizes
    streams_toc_count.count = streams_script.size();
    streams_toc_count.uncompressed_size = streams_toc_count.count * sizeof(struct stream);

    // Reserve the required memory. Must be freed later
    streams_toc = (stream *)malloc(streams_toc_count.uncompressed_size);

    //
    // Set the data
    //

    // If the stream is not initialized, do it
    if (!streams_toc) {
        fprintf(stderr, "There was an error reserving the memory for the stream toc.\n");
        return ECMTOOL_BUFFER_MEMORY_ERROR;
    }
    // Set the data
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        streams_toc[i].compression = streams_script[i].stream_data.compression;
        streams_toc[i].end_sector = streams_script[i].stream_data.end_sector;
        streams_toc[i].out_end_position = streams_script[i].stream_data.out_end_position;
        streams_toc[i].type = streams_script[i].stream_data.type;
    }

    return ECMTOOL_OK;
}


static ecmtool_return_code task_to_sectors_header (
    sector *&sectors_toc,
    sec_str_size &sectors_toc_count,
    std::vector<stream_script> &streams_script
) {
    // Set the sizes
    sectors_toc_count.count = 1;
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        sectors_toc_count.count += streams_script[i].sectors_data.size();
    }
    sectors_toc_count.uncompressed_size = sectors_toc_count.count * sizeof(struct sector);

    // Reserve the required memory. Must be freed later
    sectors_toc = (sector *)malloc(sectors_toc_count.uncompressed_size);

    //
    // Set the data
    //

    // If the stream is not initialized, do it
    if (!sectors_toc) {
        fprintf(stderr, "There was an error reserving the memory for the sectors toc.\n");
        return ECMTOOL_BUFFER_MEMORY_ERROR;
    }
    // Set the data
    uint32_t current_sector_data = 0;
    for (uint32_t i = 0; i < streams_script.size(); i++) {
        for (uint32_t j = 0; j < streams_script[i].sectors_data.size(); j++) {
            sectors_toc[current_sector_data].mode = streams_script[i].sectors_data[j].mode;
            sectors_toc[current_sector_data].sector_count = streams_script[i].sectors_data[j].sector_count;
            current_sector_data++;
        }
    }

    return ECMTOOL_OK;
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
    stream *streams_toc,
    sec_str_size &streams_toc_count,
    sector *sectors_toc,
    sec_str_size &sectors_toc_count,
    std::vector<stream_script> &streams_script
) {
    size_t actual_sector = 0;
    uint32_t actual_sector_pos = 0;

    for (uint32_t i = 0; i < streams_toc_count.count; i++) {
        streams_script.push_back(stream_script());
        streams_script.back().stream_data = streams_toc[i];

        while (actual_sector < streams_toc[i].end_sector) {
            if (actual_sector_pos > sectors_toc_count.count) {
                // The streams sectors doesn't fit the sectors count
                // Headers could be corrupted
                fprintf(stderr, "There was an error generating the script. Maybe the header is corrupted.\n");
                return ECMTOOL_CORRUPTED_STREAM;
            }
            // Append the sector data to the current stream
            streams_script.back().sectors_data.push_back(sectors_toc[actual_sector_pos]);

            actual_sector += sectors_toc[actual_sector_pos].sector_count;
            actual_sector_pos++;
        }

        if (actual_sector > streams_toc[i].end_sector) {
            // The actual sector must be equal to last sector in stream, otherwise could be corrupted
            fprintf(stderr, "There was an error converting the TOC header to script.\n");
            //printf("Actual: %d - Stream End: %d.\n", actual_sector, streams_toc[i].end_sector);
            return ECMTOOL_PROCESSING_ERROR;
        }
    }

    return ECMTOOL_OK;
}


int compress_header (
    uint8_t *dest,
    uint32_t &destLen,
    uint8_t *source,
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
    banner();
    fprintf(stderr, 
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
        "    -p/--sectors-per-block <sectors>\n"
        "           Add a end of block mark every X sectors in a seekable file. Max 255.\n"
        "    -f/--force\n"
        "           Force to ovewrite the output file\n"
        "    -k/--keep-output\n"
        "           Keep the output when something went wrong, otherwise will be removed on error.\n"
    );
}


static void summary(
    std::vector<uint32_t> *sectors_type,
    ecm_options *options,
    sector_tools *sTools,
    size_t compressed_size
) {
    uint16_t optimized_sector_sizes[13];
    // Reference to sectors_type
    std::vector<uint32_t>& sectors_type_ref = *sectors_type;

    // Calculate the size per sector type
    for (uint8_t i = 1; i < 13; i++) {
        size_t bytes_to_read = 0;
        // Getting the sector size prior to read, to read the real sector size and avoid to fseek every time
        sTools->encoded_sector_size(
            (sector_tools_types)i,
            bytes_to_read,
            options->optimizations
        );
        optimized_sector_sizes[i] = bytes_to_read;
    }

    // Total sectors
    uint32_t total_sectors = 0;
    for (uint8_t i = 1; i < 13; i++) {
        total_sectors += sectors_type_ref[i];
    }

    // Total size
    size_t total_size = total_sectors * 2352;

    // ECM size without compression
    size_t ecm_size = 0;
    for (uint8_t i = 1; i < 13; i++) {
        ecm_size += sectors_type_ref[i] * optimized_sector_sizes[i];
    }

    fprintf(stdout, "\n\n");
    fprintf(stdout, " ECM cleanup sumpary\n");
    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, " Type               Sectors         In Size        Out Size\n");
    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, "CDDA ............... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[1], MB(sectors_type_ref[1] * 2352), MB(sectors_type_ref[1] * optimized_sector_sizes[1])); 
    fprintf(stdout, "CDDA Gap ........... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[2], MB(sectors_type_ref[2] * 2352), MB(sectors_type_ref[2] * optimized_sector_sizes[2]));
    fprintf(stdout, "Mode 1 ............. %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[3], MB(sectors_type_ref[3] * 2352), MB(sectors_type_ref[3] * optimized_sector_sizes[3]));
    fprintf(stdout, "Mode 1 Gap ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[4], MB(sectors_type_ref[4] * 2352), MB(sectors_type_ref[4] * optimized_sector_sizes[4]));
    fprintf(stdout, "Mode 1 RAW ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[5], MB(sectors_type_ref[5] * 2352), MB(sectors_type_ref[5] * optimized_sector_sizes[5]));
    fprintf(stdout, "Mode 2 ............. %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[6], MB(sectors_type_ref[6] * 2352), MB(sectors_type_ref[6] * optimized_sector_sizes[6]));
    fprintf(stdout, "Mode 2 Gap ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[7], MB(sectors_type_ref[7] * 2352), MB(sectors_type_ref[7] * optimized_sector_sizes[7]));
    fprintf(stdout, "Mode 2 XA1 ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[8], MB(sectors_type_ref[8] * 2352), MB(sectors_type_ref[8] * optimized_sector_sizes[8]));
    fprintf(stdout, "Mode 2 XA1 Gap ..... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[9], MB(sectors_type_ref[9] * 2352), MB(sectors_type_ref[9] * optimized_sector_sizes[9]));
    fprintf(stdout, "Mode 2 XA2 ......... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[10], MB(sectors_type_ref[10] * 2352), MB(sectors_type_ref[10] * optimized_sector_sizes[10]));
    fprintf(stdout, "Mode 2 XA2 Gap ..... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[11], MB(sectors_type_ref[11] * 2352), MB(sectors_type_ref[11] * optimized_sector_sizes[11]));
    fprintf(stdout, "Unknown data ....... %6d ...... %6.2fMB ...... %6.2fMB\n", sectors_type_ref[12], MB(sectors_type_ref[12] * 2352), MB(sectors_type_ref[12] * optimized_sector_sizes[12]));
    fprintf(stdout, "-------------------------------------------------------------\n");
    fprintf(stdout, "Total .............. %6d ...... %6.2fMb ...... %6.2fMb\n", total_sectors, MB(total_size), MB(ecm_size));
    fprintf(stdout, "ECM reduction (input vs ecm) ..................... %2.2f%%\n", (1.0 - ((float)ecm_size / total_size)) * 100);
    fprintf(stdout, "\n\n");

    fprintf(stdout, " Compression Sumary\n");
    fprintf(stdout, "-------------------------------------------------------------\n");
    fprintf(stdout, "Compressed size (output) ............... %3.2fMB\n", MB(compressed_size));
    fprintf(stdout, "Compression ratio (ecm vs output)....... %2.2f%%\n", abs((1.0 - ((float)compressed_size / ecm_size)) * 100));
    fprintf(stdout, "\n\n");

    fprintf(stdout, " Output summary\n");
    fprintf(stdout, "-------------------------------------------------------------\n");
    fprintf(stdout, "Total reduction (input vs output) ...... %2.2f%%\n", abs((1.0 - ((float)compressed_size / total_size)) * 100));
}