#include "ecmtools_base.h"


ecmtools::base::base(std::string filename, std::fstream::openmode file_open_mode) {
    file.open(filename, file_open_mode);

    // Check if file is oppened
    char dummy;
    if (file.read(&dummy, 0)) {
        _is_open = true;

        char file_format[4];
        file.read(file_format, 4);
        if (file.fail()) {
            printf("There was an error reading the file (Header)\n");
            _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
        }
        else {
            if (
                file_format[0] == 'E' &&
                file_format[1] == 'C' &&
                file_format[2] == 'M' &&
                file_format[3] == ECMTOOLS_FILE_VERSION
            ) {
                // File is an ECM file
                _is_ecm = true;                

                // Read the TOC
                file.read(reinterpret_cast<char*>(&_file_toc_pos), sizeof(_file_toc_pos));
                if (file.fail()) {
                    printf("There was an error reading the file (TOC Pos)\n");
                    _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
                }
                else {
                    if (_file_toc_pos == 0) {
                        printf("There is no TOC block\n");
                        file.seekg(0, std::ios_base::end);
                        _file_toc_pos = file.tellg();
                    }
                    else {
                        // Read the ecm toc block header
                        ecmtools_block_header toc_header;
                        file.seekg(_file_toc_pos);
                        file.read(reinterpret_cast<char*>(&toc_header), sizeof(toc_header));
                        if (file.fail() || toc_header.type != ECMTOOLS_BLOCK_TOC) {
                            printf("There was an error reading the file (TOC: %d)\n", toc_header.type);
                            _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
                            return;
                        }

                        // Reserve the size o the file_toc                        
                        uint32_t file_toc_entries = toc_header.real_block_size / sizeof(struct ecmtools_toc_block);
                        file_toc.resize(file_toc_entries);

                        if (toc_header.compression) {
                            // Toc header is compressed. Decompressing it
                            char * toc_data_z = reinterpret_cast<char*>(malloc(toc_header.block_size));
                            if (!toc_data_z) {
                                _file_status = ECMTOOLS_STATUS_MEMORY_ERROR;
                                return;
                            }
                            
                            // Read the compressed data
                            file.read(toc_data_z, toc_header.block_size);
                            if (file.fail()) {
                                printf("There was an error reading the file (TOC Data compressed)\n");
                                _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
                                return;
                            }

                            // Decompress it
                            block_decompress(
                                reinterpret_cast<uint8_t*>(file_toc.data()),
                                toc_header.real_block_size,
                                reinterpret_cast<uint8_t*>(toc_data_z),
                                toc_header.block_size
                            );
                            // Free the resources
                            free(toc_data_z); 
                        }
                        else {
                            // Toc header is NOT compressed. Reading it directly
                            // Read the data
                            file.read(reinterpret_cast<char*>(file_toc.data()), toc_header.real_block_size);

                            if (file.fail()) {
                                printf("There was an error reading the file (TOC Data not compressed)\n");
                                _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }
}


ecmtools::base::~base() {
    if (is_open()) {
        file.close();
    }
}


/**
 * @brief This function searchs an hole in file to store the new block.
 * 
 * @param block_index The index of the block that will be stored. If the block is new -1.
 * @param block_size The real size of the block that will be stored
 * @return uint64_t Returns the position where the block can be stored
 */
uint64_t ecmtools::base::find_space_for_block(int16_t block_index, uint64_t block_size) {

    // First we will try to store the block in the same position if fits.
    if (block_index >= 0) {
        uint64_t current_position = file_toc[block_index].start_pos;
        uint64_t next_position = 0xFFFFFFFFFFFFFFFF; // We set the maximum size to force the first match to be stored
        
        // Find the closest next block
        for (uint8_t i = 0; i < file_toc.size(); i++) {
            if (file_toc[i].type != ECMTOOLS_BLOCK_TOC && file_toc[i].start_pos > current_position && file_toc[i].start_pos < next_position) {
                next_position = file_toc[i].start_pos;
            }
        }

        // If next_position was not updated, then the block is the last or only the TOC block is after it
        if (next_position == 0xFFFFFFFFFFFFFFFF) {
            return current_position;
        }

        // If the available space is enought to store the data, then return the original block position
        uint64_t available_space = next_position - current_position;
        if (available_space >= block_size) {
            return current_position;
        }
    }

    // If the new block cannot be stored in the same position, we will find a new one
    // First we will sort the toc to simplify the search
    std::sort(file_toc.begin(), file_toc.end());

    // Then we will check if a hole exists between blocks with enought space
    for (uint8_t i = 0; i < (file_toc.size() - 1); i++) {
        uint64_t free_space = file_toc[i + 1].start_pos - file_toc[i].start_pos - file_toc[i].size;

        if (free_space >= block_size) {
            return file_toc[i].start_pos + file_toc[i].size;
        }
    }

    // If none of the above is able to find a hole, then use the EOF Block position A.K.A TOC Position
    // TOC Block will be rewritten and then must be written again at end of file (check it on write function)
    return _file_toc_pos;
}

/**
 * @brief Clear the file status and set it to ECMTOOLS_STATUS_OK.
 * 
 */
void ecmtools::base::status_clear(){
    _file_status = ECMTOOLS_STATUS_OK;
}


/**
 * @brief Loads the metadata from the ECM file.
 * 
 * @param force_reload Force to reload the metadata if it already was loaded.
 * @return ECMTOOLS_STATUS_OK if no error, otherwise any other ecmtools_status.
 */
ecmtools::ecmtools_status ecmtools::base::load_metadata(bool force_reload) {
    // If metadata is already loaded
    if (!force_reload && _metadata_loaded) {
        _file_status = ECMTOOLS_STATUS_METADATA_ALREADY_LOADED;
        return _file_status;
    }

    find_space_for_block(0, 200);

    // Locating the metadata block position
    uint64_t metadata_position = 0;
    for (uint8_t i = 0; i < file_toc.size(); i++) {
        if (file_toc[i].type == ECMTOOLS_BLOCK_METADATA) {
            metadata_position = file_toc[i].start_pos;
            break;
        }
    }

    // If no metadata block, return
    if (metadata_position == 0) {
        _file_status = ECMTOOLS_STATUS_NO_METADATA;
        // Doesn't have metadata, but was loaded
        _metadata_loaded = true;
        return _file_status;
    }

    // Read the metadata block header
    ecmtools_block_header metadata_block_header;
    file.seekg(metadata_position);
    file.read((char*)&metadata_block_header, sizeof(metadata_block_header));
    if (file.fail()) {
        printf("There was an error reading the file (Metadata Header)\n");
        _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
        return _file_status;
    }

    // New memory buffer for metadata data
    char * metadata_data;
    try {
        metadata_data = new char(metadata_block_header.real_block_size);
    }
    catch (...) {
        _file_status = ECMTOOLS_STATUS_MEMORY_ERROR;
        return _file_status;
    }

    // Decompress the data if required, otherwise read it directly
    if(metadata_block_header.compression) {
        // New memory buffer to read the compressed data
        char * metadata_data_z;
        try {
            metadata_data = new char(metadata_block_header.block_size);
        }
        catch (...) {
            _file_status = ECMTOOLS_STATUS_MEMORY_ERROR;
            delete metadata_data;
            return _file_status;
        }

        // Read the compressed data
        try {
            file.read(metadata_data_z, metadata_block_header.block_size);
        }
        catch (const std::ifstream::failure& e) {
            _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
            delete metadata_data;
            delete metadata_data_z;
            return _file_status;
        }

        // Decompress the data
        if (block_decompress(
            reinterpret_cast<uint8_t*>(metadata_data),
            metadata_block_header.real_block_size,
            reinterpret_cast<uint8_t*>(metadata_data_z),
            metadata_block_header.block_size
        )) {
            _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
            delete metadata_data;
            delete metadata_data_z;
            return _file_status;
        }

        // Free the resources
        delete metadata_data_z;
    }
    else {
        try {
            file.read(metadata_data, metadata_block_header.real_block_size);
        }
        catch (const std::ifstream::failure& e) {
            return ECMTOOLS_STATUS_ECM_FILE_ERROR;
        }
        
    }

    // With the data readed, is time to process it
    uint64_t current_pos = 0;
    while (current_pos < metadata_block_header.real_block_size) {
        // Read a new block
        ecmtools_metadata_block metadata_block;
        uint32_t readed = 0;
        metadata_block.set_data(metadata_data + current_pos, readed);

        // Store the block data into the object
        switch(metadata_block.type) {
            /* Not supported yet
            case(ECMTOOLS_METADATA_COVER) {
                file_metadata.cover;
            }
            */
           case ECMTOOLS_METADATA_TITLE:
                file_metadata.title = metadata_block.text;
                break;
           ;;

           case ECMTOOLS_METADATA_ID:
                file_metadata.id = metadata_block.text;
                break;
           ;;

           case ECMTOOLS_METADATA_RELEASE_DATE:
                file_metadata.release_date = metadata_block.text;
                break;
           ;;

           case ECMTOOLS_METADATA_DEVELOPER:
                file_metadata.developer = metadata_block.text;
                break;
           ;;

           case ECMTOOLS_METADATA_PUBLISHER:
                file_metadata.publisher = metadata_block.text;
                break;
           ;;

           case ECMTOOLS_METADATA_GENRE:
                file_metadata.genre = metadata_block.text;
                break;
           ;;
        }

        current_pos += readed;
    }


    // Delete the metadata data buffer
    delete metadata_data;

    // Metadata loaded correctly
    _metadata_loaded = true;
    _file_status = ECMTOOLS_STATUS_OK;
    return _file_status;
}


/**
 * @brief Allows to compress an ECM block using zlib compression
 * 
 * @param dest Destination stream to store the compressed data
 * @param destLen Destination stream size. Updated with the remaining space after compressing.
 * @param source Source data to compress.
 * @param sourceLen Source data length
 * @param level Compression level 0-9 (default 9)
 * @return 0 if all was OK, or any other value if there was any error (Zlib library values)
 */
int ecmtools::base::block_compress (
    uint8_t *dest,
    uint64_t &destLen,
    uint8_t *source,
    uint64_t sourceLen,
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


/**
 * @brief Allows to decompress an ECM block compressed using zlib compression
 * 
 * @param dest Destination stream to store the uncompressed data
 * @param destLen Destination stream size. Updated with the remaining space after decompressing.
 * @param source Source zlib data to decompress.
 * @param sourceLen Source zlib data length
 * @return 0 if all was OK, or any other value if there was any error (Zlib library values)
 */
int ecmtools::base::block_decompress (
    uint8_t *dest,
    uint64_t &destLen,
    uint8_t *source,
    uint64_t sourceLen
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