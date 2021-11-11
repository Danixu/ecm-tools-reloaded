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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#include "zlib.h"
#include "simple_logger.h"
//#include "sector_tools.h"

#ifndef ECMTOOLS_H
#define ECMTOOLS_H

namespace ecmtools
{
    const char ECMTOOLS_FILE_VERSION = 3;

    enum ecmtools_status : uint8_t {
        ECMTOOLS_STATUS_OK = 0, // Everything is OK
        ECMTOOLS_STATUS_ECM_FILE_ERROR, // There was an error reading/writing an ECM file
        ECMTOOLS_STATUS_FILE_ERROR, // There was an error reading/writing a no ECM file
        ECMTOOLS_STATUS_MEMORY_ERROR,
        ECMTOOLS_STATUS_NO_METADATA,
        ECMTOOLS_STATUS_METADATA_ALREADY_LOADED,
        

    };

    enum ecmtools_block_type : uint8_t {
        ECMTOOLS_BLOCK_DELETED = 0,
        ECMTOOLS_BLOCK_METADATA,
        ECMTOOLS_BLOCK_DISK_IMAGE,
        ECMTOOLS_BLOCK_IMAGE_PATCH,
        ECMTOOLS_BLOCK_FILE,
        ECMTOOLS_BLOCK_TOC
    };

    enum ecmtools_metadata : uint8_t {
        ECMTOOLS_METADATA_COVER = 0,
        ECMTOOLS_METADATA_TITLE,
        ECMTOOLS_METADATA_ID,
        ECMTOOLS_METADATA_RELEASE_DATE,
        ECMTOOLS_METADATA_DEVELOPER,
        ECMTOOLS_METADATA_PUBLISHER,
        ECMTOOLS_METADATA_GENRE
    };

    enum ecmtools_compression : uint8_t {
        ECMTOOLS_COMPRESSION_NONE = 0,
        ECMTOOLS_COMPRESSION_ZLIB,
        ECMTOOLS_COMPRESSION_LZMA,
        ECMTOOLS_COMPRESSION_LZ4,
        ECMTOOLS_COMPRESSION_FLAC
    };

    #pragma pack(push, 1)
    /**
     * @brief Header to every block in ecm file.
     * 
     */
    struct ecmtools_block_header {
        uint8_t type;
        uint8_t compression;
        uint64_t block_size;
        uint64_t real_block_size;
    };

    /**
     * @brief ECM TOC sub-block in ECM TOC bloc
     * 
     */
    struct ecmtools_toc_block {
        uint8_t type;
        uint8_t image_id;
        uint64_t start_pos;
        uint64_t size;

        bool operator < (const ecmtools_toc_block& block) const
        {
            return (start_pos < block.start_pos);
        }
    };

    /**
     * @brief ECM Metadata block sub-block (must be unique in type)0
     * 
     */
    struct ecmtools_metadata_block {
        uint8_t type;
        uint32_t length;
        std::string text;

        void set_text(std::string txt) {
            text = txt;
            length = txt.length();
        }

        /**
         * @brief Get the data object in bytes. First byte will be the object type
         * 
         * @param out Output buffer to store the object data (char *)
         * @param out_length Output buffer length. Will be updated with the left bytes
         * @return 0 if everything was OK and 1 if data doesn't fit into buffer 
         */
        int get_data(char *out, uint32_t &out_length) {
            if (out_length < (text.length() + sizeof(length))) {
                // Data doesn't fit into the output buffer
                return 1;
            }

            // Copy the data into the output buffer
            std::memcpy(out, &type, sizeof(type));
            std::memcpy(out, &length, sizeof(length));
            std::memcpy(out + sizeof(length), text.data(), length);

            // Update the length with the left bytes
            out_length -= sizeof(type) + sizeof(length) + length;

            return 0;
        }

        /**
         * @brief Set the object data. First byte will be the object type and the the data.
         * 
         * @param in In buffer with the object data
         * @param in_length In buffer length
         */
        void set_data(char *in, uint32_t &in_readed) {
            // Copy the data from the input buffer to the object
            std::memcpy(&type, in, sizeof(type));
            std::memcpy(&length, in + sizeof(type), sizeof(length));
            text.resize(length);
            std::memcpy((void *)text.data(), in + sizeof(type) + sizeof(length), length);

            // Set the readed data length
            in_readed = sizeof(type) + sizeof(length) + length;
        }
    };
    #pragma pack(pop)

    struct ecmtools_file_metadata {
        uint8_t * cover;
        std::string title;
        std::string id;
        std::string release_date;
        std::string developer;
        std::string publisher;
        std::string genre;
    };

    class base
    {
        protected:
            // Methods
            base(std::string filename, std::fstream::openmode file_open_mode = std::fstream::in | std::fstream::binary);
            ~base();
            simplelogger::logger log = simplelogger::logger(simplelogger::SIMPLELOGGER_LEVEL_DEBUG); // Used to log to console
            uint64_t find_space_for_block(int16_t block_index, uint64_t block_size);
            int write_block(char * block_data, uint64_t block_size, ecmtools_compression compression = ECMTOOLS_COMPRESSION_ZLIB);

            // Variables
            bool _is_open = false;
            bool _is_ecm = false;
            uint64_t _file_toc_pos;
            std::fstream file;
            ecmtools_status _file_status = ECMTOOLS_STATUS_OK;
            

        private:
            // Methods
            int block_compress (
                uint8_t *dest,
                uint64_t &destLen,
                uint8_t *source,
                uint64_t sourceLen,
                int level = 9
            );

            int block_decompress (
                uint8_t *dest,
                uint64_t &destLen,
                uint8_t *source,
                uint64_t sourceLen
            );

            // Variables
            bool _metadata_loaded = false;

        public:
            // Methods
            bool is_open(){ return _is_open; };
            bool is_ecm() { return _is_ecm; };
            ecmtools_status status_get() { return _file_status; };
            void status_clear();

            ecmtools_status load_metadata(bool force_reload = false);

            // Variables
            std::vector<ecmtools_toc_block> file_toc;
            ecmtools_file_metadata file_metadata;
    };
}

#endif