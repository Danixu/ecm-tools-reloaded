#include "ecmtools_writer.h"

ecmtools::writer::writer(std::string filename, bool replace) : base(filename, std::fstream::in | std::fstream::out | std::fstream::binary) {
    if (_file_status) {
        // There was an error in base class while opening the file
        // Nothing will be done
        return;
    }
    if (!_is_ecm && replace) {
        // If file is not an ECM file, must be initialized

        // Reopen the file but truncating
        file.close();
        file.open(
            filename,
            std::fstream::in | std::fstream::out | std::fstream::binary | std::fstream::trunc
        );

        // Write the file header
        char file_header[] = { 'E', 'C', 'M', 3, 0, 0, 0, 0, 0, 0, 0, 0}; // Main Header (ECM3 + 8 zeroed bytes for TOC position)
        file.write(file_header, 12);

        _is_ecm = true;
    }
    else if (_is_ecm){
        _is_open = true;
        _file_status = ECMTOOLS_STATUS_OK;
    }
    else {
        _is_ecm = false;
        _file_status = ECMTOOLS_STATUS_ECM_FILE_ERROR;
    }
}


/**
 * @brief Write the stored metadata into the file. If there's space, wil be written, if not will be written at the end.
 * 
 * @return ECMTOOLS_STATUS_OK if all was OK, otherwise a positive value (ecmtools::ecmtools_status)
 */
ecmtools::ecmtools_status ecmtools::writer::write_metadata() {
    // Locating the metadata block position
    uint64_t metadata_position = 0;
    for (uint8_t i = 0; i < file_toc.size(); i++) {
        if (file_toc[i].type == ECMTOOLS_BLOCK_METADATA) {
            metadata_position = file_toc[i].start_pos;
            break;
        }
    }

    // If no metadata block, store it at TOC position (end of file)
    if (metadata_position == 0) {
        metadata_position = _file_toc_pos;
    }
    else {

    }

    return ECMTOOLS_STATUS_OK;
}