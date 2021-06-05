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

#include "sector_tools.h"

sector_tools::sector_tools() {
    // Class initialzer
    eccedc_init();
}

uint8_t sector_tools::detect(uint8_t* sector) {
    if(
        sector[0x000] == 0x00 && // sync (12 bytes)
        sector[0x001] == 0xFF &&
        sector[0x002] == 0xFF &&
        sector[0x003] == 0xFF &&
        sector[0x004] == 0xFF &&
        sector[0x005] == 0xFF &&
        sector[0x006] == 0xFF &&
        sector[0x007] == 0xFF &&
        sector[0x008] == 0xFF &&
        sector[0x009] == 0xFF &&
        sector[0x00A] == 0xFF &&
        sector[0x00B] == 0x00
    ) {
        // Sector is a MODE1/MODE2 sector
        if (
            sector[0x00F] == 0x01 && // mode (1 byte)
            sector[0x814] == 0x00 && // reserved (8 bytes)
            sector[0x815] == 0x00 &&
            sector[0x816] == 0x00 &&
            sector[0x817] == 0x00 &&
            sector[0x818] == 0x00 &&
            sector[0x819] == 0x00 &&
            sector[0x81A] == 0x00 &&
            sector[0x81B] == 0x00
        ) {
            //  The sector is surelly MODE1 but we will check the EDC
            if(
                ecc_checksector(
                    sector + 0xC,
                    sector + 0x10,
                    sector + 0x81C
                ) &&
                edc_compute(0, sector, 0x810) == get32lsb(sector + 0x810)
            ) {
                if(is_gap(sector + 0x010, 0x800)) {
                    return STT_MODE1_GAP;
                }
                else {
                    return STT_MODE1; // Mode 1
                }
            }

            // If EDC doesn't match, then the sector is damaged or is unknown
            return STT_UNKNOWN;
        }
        else if (
            sector[0x00F] == 0x02 // mode (1 byte)
        ) {
            //  The sector is MODE2, and now we will detect what kind

            //
            // Might be Mode 2, Form 1 or 2
            //
            if(
                ecc_checksector(
                    zeroaddress,
                    sector + 0x010,
                    sector + 0x81C
                ) &&
                edc_compute(0, sector + 0x010, 0x808) == get32lsb(sector + 0x818)
            ) {
                if(is_gap(sector + 0x018, 0x800)) {
                    return STT_MODE2_1_GAP;
                }
                else {
                    return STT_MODE2_1; //  Mode 2, Form 1
                }
            }
            //
            // Might be Mode 2, Form 2
            //
            if(
                edc_compute(0, sector + 0x010, 0x91C) == get32lsb(sector + 0x92C)
            ) {

                if(is_gap(sector + 0x018, 0x914)) {
                    return STT_MODE2_2_GAP;
                }
                else {
                    return STT_MODE2_2; // Mode 2, Form 2
                }
            }

            // No XA sector detected, so might be normal MODE2
            if(is_gap(sector + 0x010, 0x920)) {
                return STT_MODE2_GAP;
            }
            else {
                return STT_MODE2;
            }            
        }
    }
    else {
        // Sector is not recognized, so might be a CDDA sector
        if(is_gap(sector, 0x930)) {
            return STT_CDDA_GAP;
        }
        else {
            return STT_CDDA;
        }

        // If all sectors are NULL then the sector is a CDDA GAP
        return STT_CDDA_GAP;
    }

    return STT_UNKNOWN;
}


////////////////////////////////////////////////////////////////////////////////
// Detects if sectors are zeroed (GAP)
bool sector_tools::is_gap(uint8_t *sector, uint32_t length) {
    for (uint16_t i = 0; i < length; i++) {
        if ((sector[i]) != 0x00) {
            return false; // Sector contains data, so is not a GAP
        }
    }

    return true;
}


////////////////////////////////////////////////////////////////////////////////
uint32_t sector_tools::get32lsb(const uint8_t* src) {
    return
        (((uint32_t)(src[0])) <<  0) |
        (((uint32_t)(src[1])) <<  8) |
        (((uint32_t)(src[2])) << 16) |
        (((uint32_t)(src[3])) << 24);
}

////////////////////////////////////////////////////////////////////////////////
//
// LUTs used for computing ECC/EDC
//
void sector_tools::eccedc_init(void) {
    size_t i;
    for(i = 0; i < 256; i++) {
        uint32_t edc = i;
        size_t j = (i << 1) ^ (i & 0x80 ? 0x11D : 0);
        ecc_f_lut[i] = j;
        ecc_b_lut[i ^ j] = i;
        for(j = 0; j < 8; j++) {
            edc = (edc >> 1) ^ (edc & 1 ? 0xD8018001 : 0);
        }
        edc_lut[i] = edc;
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Compute EDC for a block
//
uint32_t sector_tools::edc_compute(
    uint32_t edc,
    const uint8_t* src,
    size_t size
) {
    for(; size; size--) {
        edc = (edc >> 8) ^ edc_lut[(edc ^ (*src++)) & 0xFF];
    }
    return edc;
}

////////////////////////////////////////////////////////////////////////////////
//
// Check ECC block (either P or Q)
// Returns true if the ECC data is an exact match
//
int8_t sector_tools::ecc_checkpq(
    const uint8_t* address,
    const uint8_t* data,
    size_t major_count,
    size_t minor_count,
    size_t major_mult,
    size_t minor_inc,
    const uint8_t* ecc
) {
    size_t size = major_count * minor_count;
    size_t major;
    for(major = 0; major < major_count; major++) {
        size_t index = (major >> 1) * major_mult + (major & 1);
        uint8_t ecc_a = 0;
        uint8_t ecc_b = 0;
        size_t minor;
        for(minor = 0; minor < minor_count; minor++) {
            uint8_t temp;
            if(index < 4) {
                temp = address[index];
            } else {
                temp = data[index - 4];
            }
            index += minor_inc;
            if(index >= size) { index -= size; }
            ecc_a ^= temp;
            ecc_b ^= temp;
            ecc_a = ecc_f_lut[ecc_a];
        }
        ecc_a = ecc_b_lut[ecc_f_lut[ecc_a] ^ ecc_b];
        if(
            ecc[major              ] != (ecc_a        ) ||
            ecc[major + major_count] != (ecc_a ^ ecc_b)
        ) {
            return 0;
        }
    }
    return 1;
}

//
// Check ECC P and Q codes for a sector
// Returns true if the ECC data is an exact match
//
int8_t sector_tools::ecc_checksector(
    const uint8_t *address,
    const uint8_t *data,
    const uint8_t *ecc
) {
    return
        ecc_checkpq(address, data, 86, 24,  2, 86, ecc) &&      // P
        ecc_checkpq(address, data, 52, 43, 86, 88, ecc + 0xAC); // Q
}