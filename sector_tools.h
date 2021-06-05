////////////////////////////////////////////////////////////////////////////////
//
// Common headers for Command-Line Pack programs
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
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 01
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
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
// 0010h [---DATA...
// ...
// 0920h                                     ...DATA---]
// -----------------------------------------------------
//
// Mode 2 (XA), form 1
// -----------------------------------------------------
//        0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
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
// 0000h 00 FF FF FF FF FF FF FF FF FF FF 00 [-ADDR-] 02
// 0010h [--FLAGS--] [--FLAGS--] [---DATA...
// ...
// 0920h                         ...DATA---] [---EDC---]
// -----------------------------------------------------
//
// ADDR:  Sector address, encoded as minutes:seconds:frames in BCD
// FLAGS: Used in Mode 2 (XA) sectors describing the type of sector; repeated
//        twice for redundancy
// DATA:  Area of the sector which contains the actual data itself
// EDC:   Error Detection Code
// ECC:   Error Correction Code
//

#include <cstddef>
#include <stdint.h>
#include <stdio.h>

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
enum sector_tools_types {
    STT_UNKNOWN = 0,
    STT_CDDA,
    STT_CDDA_GAP,
    STT_MODE1,
    STT_MODE1_GAP,
    STT_MODE2,
    STT_MODE2_GAP,
    STT_MODE2_1,
    STT_MODE2_1_GAP,
    STT_MODE2_2,
    STT_MODE2_2_GAP
};

//
// sector_tools Class
//
class sector_tools {
    public:
        // Public methods
        sector_tools();
        uint8_t detect(uint8_t* sector);

        // Public attributes
        int8_t last_sector_type = -1; 

    private:
        // Private methods
        bool is_gap(uint8_t *sector, uint32_t length);
        uint32_t get32lsb(const uint8_t* src);
        void eccedc_init(void);
        uint32_t edc_compute(
            uint32_t edc,
            const uint8_t* src,
            size_t size
        );
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

        // Private attributes
        //
        const uint8_t zeroaddress[4] = {0, 0, 0, 0};
        // LUTs used for computing ECC/EDC
        uint8_t  ecc_f_lut[256];
        uint8_t  ecc_b_lut[256];
        uint32_t edc_lut  [256];

};