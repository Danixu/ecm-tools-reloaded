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


//////////////////////////////////////////////////////////////////
//
// Sector Detector class
//
// This class allows to detect the sector type of a CD-ROM sector
// From now is only able to detect the following sectors types:
//   * CDDA: If the sector type is unrecognized, will be threated as raw (like CDDA)
//   * CDDA_GAP: As above sector type, unrecognized type. The difference is that GAP is zeroed


////////////////////////////////////////////////////////////////////////////////
//
// Sector types
// 
// CDDA
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h [---DATA...
// ...
// 0920h                                     ...DATA---]
// -----------------------------------------------------
//
// Mode 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-MSF -] 01
// 0010h [---DATA...
// ...
// 0800h                                     ...DATA---]
// 0810h [---EDC---] 00 00 00 00 00 00 00 00 [---ECC...
// ...
// 0920h                                      ...ECC---]
// -----------------------------------------------------
//
// Mode 2: This mode is not widely used
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-MSF -] 02
// 0010h [---DATA...
// ...
// 0920h                                     ...DATA---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-MSF -] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0810h             ...DATA---] [---EDC---] [---ECC...
// ...
// 0920h                                      ...ECC---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 2
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-MSF -] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0920h                         ...DATA---] [---EDC---]
// -----------------------------------------------------
//
// MSF:  Sector address, encoded as minutes:seconds:frames in BCD
// FLAGS: Used in Mode 2 (XA) sectors describing the type of sector; repeated
//        twice for redundancy
// DATA:  Area of the sector which contains the actual data itself
// EDC:   Error Detection Code
// ECC:   Error Correction Code
//
// First sector address looks like is always 00:02:00, so we will use this number on it

#include <cstddef>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "compressor.h"

// 
// Return codes of the class methods
//
enum sector_tools_returns {
    STR_OK = 100,
    STR_ERROR_NO_ENOUGH_SECTORS
};

//
// Sector types detectable by the class
//
enum sector_tools_types : uint8_t {
    STT_UNKNOWN = 0,
    STT_CDDA,
    STT_CDDA_GAP,
    STT_MODE1,
    STT_MODE1_GAP,
    STT_MODE1_RAW,
    STT_MODE2,
    STT_MODE2_GAP,
    STT_MODE2_1,
    STT_MODE2_1_GAP,
    STT_MODE2_2,
    STT_MODE2_2_GAP,
    STT_MODEX
};

//
// Stream Compressions
//
enum sector_tools_stream_types : uint8_t {
    STST_UNKNOWN = 0,
    STST_AUDIO,
    STST_DATA
};

//
// Optimization options available
//
enum optimization_options : uint8_t {
    OO_NONE                  = 0,    // Just copy the input. Surelly will not be used
    OO_REMOVE_SYNC           = 1, // Remove sync bytes (a.k.a first 12 bytes)
    OO_REMOVE_MSF           = 1<<1, // Remove the MSF bytes
    OO_REMOVE_MODE           = 1<<2, // Remove the MODE byte
    OO_REMOVE_BLANKS         = 1<<3, // If sector type is a GAP, remove the data
    OO_REMOVE_REDUNDANT_FLAG = 1<<4, // Remove the redundant copy of FLAG bytes in Mode 2 XA sectors
    OO_REMOVE_ECC            = 1<<5, // Remove the ECC
    OO_REMOVE_EDC            = 1<<6, // Remove the EDC
    OO_REMOVE_GAP            = 1<<7  // If sector type is a GAP, remove the data
};
inline optimization_options operator|(optimization_options a, optimization_options b)
{
    return static_cast<optimization_options>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}


//
// sector_tools Class
//
class sector_tools {
    public:
        // Public methods
        sector_tools();
        static uint32_t get32lsb(const uint8_t* src);
        static void put32lsb(uint8_t* dest, uint32_t value);
        sector_tools_types detect(uint8_t* sector);
        static sector_tools_stream_types detect_stream(sector_tools_types type);
        uint32_t edc_compute(
            uint32_t edc,
            const uint8_t* src,
            size_t size
        );
        static int8_t write_type_count(
            uint8_t* outBuffer,
            sector_tools_types type,
            uint32_t count,
            uint8_t& generated_bytes
        );
        static int8_t write_stream_type_count(
            uint8_t* outBuffer,
            sector_tools_compression compression,
            sector_tools_stream_types type,
            uint32_t count,
            uint8_t& generated_bytes
        );
        static int8_t read_type_count(
            uint8_t* inBuffer,
            sector_tools_types& type,
            uint32_t& count,
            uint8_t& readed_bytes
        );
        static int8_t read_stream_type_count(
            uint8_t* inBuffer,
            sector_tools_compression compression,
            sector_tools_stream_types& type,
            uint32_t& count,
            uint8_t& readed_bytes
        );
        // sector cleaner CDDA
        static int8_t clean_sector_cdda(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector cleaner Mode 1
        static int8_t clean_sector_mode1(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector cleaner Mode 2
        static int8_t clean_sector_mode2(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector cleaner Mode 2 XA 1
        static int8_t clean_sector_mode2_xa1(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector cleaner Mode 2 XA 1
        static int8_t clean_sector_mode2_xa2(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector cleaner Unknown Mode
        static int8_t clean_sector_modex(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector cleaner switcher
        static int8_t clean_sector(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t& output_size,
            optimization_options options
        );
        // sector regenerator CDDA
        int8_t regenerate_sector_cdda(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t current_pos,
            uint16_t& bytes_readed,
            optimization_options options
        );
        //  sector regenerator Mode 1
        int8_t regenerate_sector_mode1(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t current_pos,
            uint16_t& bytes_readed,
            optimization_options options
        );
        //  sector regenerator Mode 2
        int8_t regenerate_sector_mode2(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t current_pos,
            uint16_t& bytes_readed,
            optimization_options options
        );
        //  sector regenerator Mode 2 XA 1
        int8_t regenerate_sector_mode2_xa1(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t current_pos,
            uint16_t& bytes_readed,
            optimization_options options
        );
        //  sector regenerator Mode 2 XA 2
        int8_t regenerate_sector_mode2_xa2(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t current_pos,
            uint16_t& bytes_readed,
            optimization_options options
        );
        //  sector regenerator Unknown mode
        int8_t regenerate_sector_modex(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint16_t current_pos,
            uint16_t& bytes_readed,
            optimization_options options
        );
        int8_t regenerate_sector(
            uint8_t* out,
            uint8_t* sector,
            sector_tools_types type,
            uint32_t sector_number,
            uint16_t& bytes_readed,
            optimization_options options
        );
        static int8_t encoded_sector_size(
            sector_tools_types type,
            size_t& output_size,
            optimization_options options
        );
        static void sector_to_time(
            uint8_t* out,
            uint32_t sector_number
        );

        // Public attributes
        int8_t last_sector_type = -1; 

    private:
        // Private methods
        bool is_gap(uint8_t *sector, uint16_t length);
        void eccedc_init(void);
        int8_t ecc_checkpq(
            const uint8_t* address,
            const uint8_t* data,
            size_t major_count,
            size_t minor_count,
            size_t major_mult,
            size_t minor_inc,
            const uint8_t* ecc
        );
        int8_t ecc_checksector(
            const uint8_t *address,
            const uint8_t *data,
            const uint8_t *ecc
        );
        void ecc_writepq(
            const uint8_t* address,
            const uint8_t* data,
            size_t major_count,
            size_t minor_count,
            size_t major_mult,
            size_t minor_inc,
            uint8_t* ecc
        );
        void ecc_writesector(
            const uint8_t *address,
            const uint8_t *data,
            uint8_t *ecc
        );

        // Private attributes
        //
        const uint8_t zeroaddress[4] = {0, 0, 0, 0};
        // LUTs used for computing ECC/EDC
        uint8_t  ecc_f_lut[256];
        uint8_t  ecc_b_lut[256];
        uint32_t edc_lut  [256];
};