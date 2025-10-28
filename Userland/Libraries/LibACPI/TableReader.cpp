/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "TableReader.h"
#include "Definitions.h"
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace ACPI {
bool TableReader::is_lead_name_char(u8 c)
{
    return (c == '_') || (c >= 'A' && c <= 'Z');
}

bool TableReader::is_name_char(u8 c)
{
    return is_lead_name_char(c) || (c >= '0' && c <= '9');
}

u8 TableReader::peek()
{
    if (m_position >= length()) {
        return 0;
    }
    return m_view[m_position];
}

void TableReader::read_into(size_t len, AK::Vector<u8>& buffer)
{
    for (size_t i = 0; i < len; i++) {
        buffer[i] = byte();
    }
}

u8 TableReader::byte()
{
    if (m_position >= length()) {
        return 0;
    }
    return m_view[m_position++];
}

u16 TableReader::word()
{
    return byte() | (byte() << 8);
}

u32 TableReader::dword()
{
    return byte() | (byte() << 8) | (byte() << 16) | (byte() << 24);
}

AK::ErrorOr<AK::String> TableReader::string()
{
    size_t start = m_position;
    u8 b = byte();
    do {
        if (b == 0 || b > 0x7f) {
            return Error::from_string_literal("Invalid character in string!");
        }

        b = byte();
    } while (b != 0);

    auto view = m_view.substring_view(start, m_position - start);
    return AK::String::from_utf8_with_replacement_character(view, AK::String::WithBOMHandling::No);
}

u64 TableReader::qword()
{
    return byte() | (byte() << 8) | (byte() << 16) | (byte() << 24) | ((uint64_t)byte() << 32) | ((uint64_t)byte() << 40) | ((uint64_t)byte() << 48) | ((uint64_t)byte() << 56);
}

u16 TableReader::opcode()
{
    u16 opcode = byte();
    if (opcode == static_cast<u16>(Prefix::ExtOpPrefix)) {
        opcode = (opcode << 8) | byte();
    }
    return opcode;
}

i64 TableReader::package_length()
{
    u8 initial = byte();
    if ((initial & 0b11000000) == 0)
        return initial;

    i64 value = (initial & 0b1111);
    u8 additional_byte_count = initial >> 6;
    int bits = 4;
    for (auto i = 0; i < additional_byte_count; i++) {
        value = value | (byte() << bits);
        bits += 8;
    }

    return value;
}

AK::ErrorOr<AK::StringView> TableReader::name_segment()
{
    auto segment = string_view(4);
    if (!TableReader::is_lead_name_char(segment[0]) || !TableReader::is_name_char(segment[1]) || !TableReader::is_name_char(segment[2]) || !TableReader::is_name_char(segment[3])) {
        warnln("[LibACPI] TableReader::name_segment: Invalid name segment '{}'!", segment);
        return Error::from_string_literal("Invalid character in name segment!");
    }

    return segment;
}

AK::StringView TableReader::string_view(size_t size)
{
    if (m_position + size >= length())
        size = length() - m_position;

    auto view = m_view.substring_view(m_position, size);
    m_position += size;
    return view;
}
u8 TableReader::generate_checksum() const
{
    u8 checksum = 0;
    for (size_t i = 0; i < length(); i++) {
        checksum += m_view[i];
    }
    return checksum;
}

}
