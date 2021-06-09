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

sector_tools_types sector_tools::detect(uint8_t* sector) {
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
            // Checking if sector is MODE 2 without XA
            if(is_gap(sector + 0x010, 0x920)) {
                return STT_MODE2_GAP;
            }
            else {
                return STT_MODE2;
            }   

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
// Detects the stream type using the sector type
sector_tools_stream_types sector_tools::detect_stream(sector_tools_types type) {
    switch (type){
        case STT_CDDA:
        case STT_CDDA_GAP:
            return STST_AUDIO;
            break;
        
        default:
            return STST_DATA;
    }
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

void sector_tools::put32lsb(uint8_t* dest, uint32_t value) {
    dest[0] = (uint8_t)(value      );
    dest[1] = (uint8_t)(value >>  8);
    dest[2] = (uint8_t)(value >> 16);
    dest[3] = (uint8_t)(value >> 24);
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
// Write ECC block (either P or Q)
//
void sector_tools::ecc_writepq(
    const uint8_t* address,
    const uint8_t* data,
    size_t major_count,
    size_t minor_count,
    size_t major_mult,
    size_t minor_inc,
    uint8_t* ecc
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
        ecc[major              ] = (ecc_a        );
        ecc[major + major_count] = (ecc_a ^ ecc_b);
    }
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

//
// Write ECC P and Q codes for a sector
//
void sector_tools::ecc_writesector(
    const uint8_t *address,
    const uint8_t *data,
    uint8_t *ecc
) {
    ecc_writepq(address, data, 86, 24,  2, 86, ecc);        // P
    ecc_writepq(address, data, 52, 43, 86, 88, ecc + 0xAC); // Q
}

////////////////////////////////////////////////////////////////////////////////
//
// Encode a type/count combo
//
// Returns nonzero on error
//
int8_t sector_tools::write_type_count(
    uint8_t* outBuffer,
    sector_tools_types type,
    uint32_t count,
    uint8_t& generated_bytes
) {
    generated_bytes = 0; // We will set the generated_bytes counter to 0

    // Generating the first byte wich contains the type and a number up to 15
    // First bit is used as a "more exists" mark
    outBuffer[generated_bytes] = ((count >= 8) << 7) | ((count & 7) << 4) | type;
    generated_bytes++; // Now we are in position 1

    count >>= 3; // We will shift the counter 3 positions to right (same as divide by 8)

    // While the counter is not 0
    while(count) {
        outBuffer[generated_bytes] = ((count >= 128) << 7) | (count & 127);
        generated_bytes++; // one more byte

        count >>= 7; // same as divide by 128
    }
    //
    // Success
    //
    return 0;
}

// Overload for stream type
int8_t sector_tools::write_type_count(
    uint8_t* outBuffer,
    sector_tools_stream_types type,
    uint32_t count,
    uint8_t& generated_bytes
) {
    generated_bytes = 0; // We will set the generated_bytes counter to 0

    // Generating the first byte wich contains the type and a number up to 15
    // First bit is used as a "more exists" mark
    outBuffer[generated_bytes] = ((count >= 32) << 7) | ((count & 31) << 2) | type;
    generated_bytes++; // Now we are in position 1

    count >>= 5; // We will shift the counter 5 positions to right (same as divide by 32)

    // While the counter is not 0
    while(count) {
        outBuffer[generated_bytes] = ((count >= 128) << 7) | (count & 127);
        generated_bytes++; // one more byte

        count >>= 7; // same as divide by 128
    }
    //
    // Success
    //
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Read a type/count combo
//
// Returns nonzero on error
//
int8_t sector_tools::read_type_count(
    uint8_t* inBuffer,
    sector_tools_types& type,
    uint32_t& count,
    uint8_t& readed_bytes
) {
    readed_bytes = 0;
    int c = inBuffer[readed_bytes];
    readed_bytes++;
    int bits = 3;

    type = (sector_tools_types)(c & 0xF);
    count = (c >> 4) & 0x7;

    while(c & 0x80) {
        c = inBuffer[readed_bytes];
        readed_bytes++;

        if(
            (bits > 31) ||
            ((uint32_t)(c & 0x7F)) >= (((uint32_t)0x80000000LU) >> (bits-1))
        ) {
            printf("Invalid sector count\n");
            return -1;
        }
        count |= ((uint32_t)(c & 0x7F)) << bits;
        bits += 7;
    }
    if(count == 0x0) {
        // End indicator
        return 0;
    }

    return 1;
}

// Overload for stream type
int8_t sector_tools::read_type_count(
    uint8_t* inBuffer,
    sector_tools_stream_types& type,
    uint32_t& count,
    uint8_t& readed_bytes
) {
    readed_bytes = 0;
    int c = inBuffer[readed_bytes];
    readed_bytes++;
    int bits = 5;

    type = (sector_tools_stream_types)(c & 0x3);
    count = (c >> 2) & 0x1F;

    while(c & 0x80) {
        c = inBuffer[readed_bytes];
        readed_bytes++;

        if(
            (bits > 31) ||
            ((uint32_t)(c & 0x7F)) >= (((uint32_t)0x80000000LU) >> (bits-1))
        ) {
            printf("Invalid sector count\n");
            return -1;
        }
        count |= ((uint32_t)(c & 0x7F)) << bits;
        bits += 7;
    }
    if(count == 0x0) {
        // End indicator
        return 0;
    }

    return 1;
}

void sector_tools::sector_to_time(
    uint8_t* out,
    uint32_t sector_number
) {
    uint8_t sectors = sector_number % 75;
    uint8_t seconds = (sector_number / 75) % 60;
    uint8_t minutes = (sector_number / 75) / 60;

    // Converting decimal to hex base 10
    // 15 -> 0x15 instead 0x0F
    out[2] = (sectors / 10 * 16) + (sectors % 10);
    out[1] = (seconds / 10 * 16) + (seconds % 10);
    out[0] = (minutes / 10 * 16) + (minutes % 10);
}


////////////////////////////////////////////////////////////////////////////////
//
// Remove non required data from a sector depending of options
//
// Returns nonzero on error
//
int8_t sector_tools::clean_sector(
    uint8_t* out,
    uint8_t* sector,
    sector_tools_types type,
    uint16_t& output_size,
    optimization_options options
) {
    output_size = 0;
    switch(type) {
        case STT_CDDA:
        case STT_CDDA_GAP:
            // CDDA are directly copied
            if (type == STT_CDDA || !(options & OO_REMOVE_GAP)) {
                memcpy(out, sector, 2352);
                output_size = 2352;
            }
            break;

        case STT_MODE1:
        case STT_MODE1_GAP:
            // SYNC bytes
            if (!(options & OO_REMOVE_SYNC)) {
                memcpy(out, sector, 0x0C);
                output_size += 0x0C;
            }
            // Address bytes
            if (!(options & OO_REMOVE_ADDR)) {
                memcpy(out + output_size, sector + 0x0C, 0x03);
                output_size += 0x03;
            }
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + output_size, sector + 0x0F, 0x01);
                output_size += 0x01;
            }
            // Data bytes
            if (type == STT_MODE1 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + output_size, sector + 0x10, 0x800);
                output_size += 0x800;
            }
            // EDC bytes
            if (!(options & OO_REMOVE_EDC)) {
                memcpy(out + output_size, sector + 0x810, 0x04);
                output_size += 0x04;
            }
            // Zeroed bytes
            if (!(options & OO_REMOVE_BLANKS)) {
                memcpy(out + output_size, sector + 0x814, 0x08);
                output_size += 0x08;
            }
            // ECC bytes
            if (!(options & OO_REMOVE_ECC)) {
                memcpy(out + output_size, sector + 0x81C, 0x114);
                output_size += 0x114;
            }
            break;

        case STT_MODE2:
        case STT_MODE2_GAP:
            // SYNC bytes
            if (!(options & OO_REMOVE_SYNC)) {
                memcpy(out, sector, 0x0C);
                output_size += 0x0C;
            }
            // Address bytes
            if (!(options & OO_REMOVE_ADDR)) {
                memcpy(out + output_size, sector + 0x0C, 0x03);
                output_size += 0x03;
            }
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + output_size, sector + 0x0F, 0x01);
                output_size += 0x01;
            }
            // Data bytes
            if (type == STT_MODE2 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + output_size, sector + 0x10, 0x920);
                output_size += 0x920;
            }
            break;

        case STT_MODE2_1:
        case STT_MODE2_1_GAP:
            // SYNC bytes
            if (!(options & OO_REMOVE_SYNC)) {
                memcpy(out, sector, 0x0C);
                output_size += 0x0C;
            }
            // Address bytes
            if (!(options & OO_REMOVE_ADDR)) {
                memcpy(out + output_size, sector + 0x0C, 0x03);
                output_size += 0x03;
            }
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + output_size, sector + 0x0F, 0x01);
                output_size += 0x01;
            }
            // Flags bytes
            if (!(options & OO_REMOVE_REDUNDANT_FLAG)) {
                memcpy(out + output_size, sector + 0x10, 0x08);
                output_size += 0x08;
            }
            else {
                memcpy(out + output_size, sector + 0x10, 0x04);
                output_size += 0x04;
            }
            // Data bytes
            if (type == STT_MODE2_1 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + output_size, sector + 0x18, 0x800);
                output_size += 0x800;
            }
            // EDC bytes
            if (!(options & OO_REMOVE_EDC)) {
                memcpy(out + output_size, sector + 0x818, 0x04);
                output_size += 0x04;
            }
            // ECC bytes
            if (!(options & OO_REMOVE_ECC)) {
                memcpy(out + output_size, sector + 0x81C, 0x114);
                output_size += 0x114;
            }
            break;

        case STT_MODE2_2:
        case STT_MODE2_2_GAP:
            // SYNC bytes
            if (!(options & OO_REMOVE_SYNC)) {
                memcpy(out, sector, 0x0C);
                output_size += 0x0C;
            }
            // Address bytes
            if (!(options & OO_REMOVE_ADDR)) {
                memcpy(out + output_size, sector + 0x0C, 0x03);
                output_size += 0x03;
            }
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + output_size, sector + 0x0F, 0x01);
                output_size += 0x01;
            }
            // Flags bytes
            if (!(options & OO_REMOVE_REDUNDANT_FLAG)) {
                memcpy(out + output_size, sector + 0x10, 0x08);
                output_size += 0x08;
            }
            else {
                memcpy(out + output_size, sector + 0x10, 0x04);
                output_size += 0x04;
            }
            // Data bytes
            if (type == STT_MODE2_2 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + output_size, sector + 0x18, 0x914);
                output_size += 0x914;
            }
            // EDC bytes
            if (!(options & OO_REMOVE_EDC)) {
                memcpy(out + output_size, sector + 0x92C, 0x04);
                output_size += 0x04;
            }
            break;     
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Regenerate removed data from sectors
//
// Returns nonzero on error
//
int8_t sector_tools::regenerate_sector(
    uint8_t* out,
    uint8_t* sector,
    sector_tools_types type,
    uint32_t sector_number,
    uint16_t& bytes_readed,
    optimization_options options
) {
    bytes_readed = 0;
    uint16_t current_pos = 0;
    // sync and address bytes in data sectors
    switch(type) {
        case STT_MODE1:
        case STT_MODE1_GAP:
        case STT_MODE2:
        case STT_MODE2_GAP:
        case STT_MODE2_1:
        case STT_MODE2_1_GAP:
        case STT_MODE2_2:
        case STT_MODE2_2_GAP:
            // SYNC bytes
            if (!(options & OO_REMOVE_SYNC)) {
                memcpy(out, sector, 12);
                bytes_readed += 0x0C;
            }
            else {
                out[0] = 0x00;
                memset(out + 1, 0xFF, 10);
                out[11] = 0x00;
            }
            current_pos += 0x0C;
            // Address bytes
            if (!(options & OO_REMOVE_ADDR)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x03);
                bytes_readed += 0x03;
            }
            else {
                sector_to_time(out + current_pos, sector_number);
            }
            current_pos += 0x03;

            break;
    }

    // The rest of the sector
    switch(type) {
        case STT_CDDA:
        case STT_CDDA_GAP:
            // CDDA are directly copied
            if (type == STT_CDDA || !(options & OO_REMOVE_GAP)) {
                memcpy(out, sector, 2352);
                bytes_readed = 2352;
            }
            else {
                memset(out, 0x00, 2352);
            }
            break;

        case STT_MODE1:
        case STT_MODE1_GAP:
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x01);
                bytes_readed += 0x01;
            }
            else {
                out[current_pos] = 0x01;
            }
            current_pos += 0x01;
            // Data bytes
            if (type == STT_MODE1 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x800);
                bytes_readed += 0x800;
            }
            else {
                memset(out + current_pos, 0x00, 0x800);
            }
            current_pos += 0x800;
            // EDC bytes
            if (!(options & OO_REMOVE_EDC)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x04);
                bytes_readed += 0x04;
            }
            else {
                put32lsb(out + current_pos, edc_compute(0, out     , 0x810));
            }
            current_pos += 0x04;
            // Zeroed bytes
            if (!(options & OO_REMOVE_BLANKS)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x08);
                bytes_readed += 0x08;
            }
            else {
                memset(out + current_pos, 0x00, 0x08);
            }
            current_pos += 0x08;
            // ECC bytes
            if (!(options & OO_REMOVE_ECC)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x114);
                bytes_readed += 0x114;
            }
            else {
                ecc_writesector(out + 0xC, out + 0x10, out + current_pos);
            }
            current_pos += 0x114;
            break;

        case STT_MODE2:
        case STT_MODE2_GAP:
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x01);
                bytes_readed += 0x01;
            }
            else {
                out[current_pos] = 0x02;
            }
            current_pos += 0x01;
            // Data bytes
            if (type == STT_MODE2 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x920);
                bytes_readed += 0x920;
            }
            else {
                memset(out + current_pos, 0x00, 0x920);
            }
            current_pos += 0x920;
            break;

        case STT_MODE2_1:
        case STT_MODE2_1_GAP:
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x01);
                bytes_readed += 0x01;
            }
            else {
                out[current_pos] = 0x02;
            }
            current_pos += 0x01;
            // Flags bytes
            if (!(options & OO_REMOVE_REDUNDANT_FLAG)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x08);
                bytes_readed += 0x08;
            }
            else {
                memcpy(out + current_pos, sector + bytes_readed, 0x04);
                bytes_readed += 0x04;
                out[current_pos + 4] = out[current_pos];
                out[current_pos + 5] = out[current_pos + 1];
                out[current_pos + 6] = out[current_pos + 2];
                out[current_pos + 7] = out[current_pos + 3];
            }
            current_pos += 0x08;
            // Data bytes
            if (type == STT_MODE2_1 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x800);
                bytes_readed += 0x800;
            }
            else {
                memset(out + current_pos, 0x00, 0x800);
            }
            current_pos += 0x800;
            // EDC bytes
            if (!(options & OO_REMOVE_EDC)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x04);
                bytes_readed += 0x04;
            }
            else {
                put32lsb(out + current_pos, edc_compute(0, out + 0x10, 0x808));
            }
            current_pos += 0x04;
            // ECC bytes
            if (!(options & OO_REMOVE_ECC)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x114);
                bytes_readed += 0x114;
            }
            else {
                ecc_writesector(zeroaddress, out + 0x10, out + current_pos);
            }
            current_pos += 0x114;
            break;

        case STT_MODE2_2:
        case STT_MODE2_2_GAP:
            // Mode bytes
            if (!(options & OO_REMOVE_MODE)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x01);
                bytes_readed += 0x01;
            }
            else {
                out[current_pos] = 0x02;
            }
            current_pos += 0x01;
            // Flags bytes
            if (!(options & OO_REMOVE_REDUNDANT_FLAG)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x08);
                bytes_readed += 0x08;
            }
            else {
                memcpy(out + current_pos, sector + bytes_readed, 0x04);
                bytes_readed += 0x04;
                out[current_pos + 4] = out[current_pos];
                out[current_pos + 5] = out[current_pos + 1];
                out[current_pos + 6] = out[current_pos + 2];
                out[current_pos + 7] = out[current_pos + 3];
            }
            current_pos += 0x08;
            // Data bytes
            if (type == STT_MODE2_2 || !(options & OO_REMOVE_GAP)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x914);
                bytes_readed += 0x914;
            }
            else {
                memset(out + current_pos, 0x00, 0x914);
            }
            current_pos += 0x914;
            // EDC bytes
            if (!(options & OO_REMOVE_EDC)) {
                memcpy(out + current_pos, sector + bytes_readed, 0x04);
                bytes_readed += 0x04;
            }
            else {
                put32lsb(out + current_pos, edc_compute(0, out + 0x10, 0x91C));
            }
            current_pos += 0x04;
            break;
    }

    return 0;
}