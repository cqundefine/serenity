/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "TableStream.h"
#include "Definitions.h"

namespace ACPI {

bool TableStream::is_lead_name_char(u8 c)
{
    return (c == '_') || (c >= 'A' && c <= 'Z');
}

bool TableStream::is_name_char(u8 c)
{
    return is_lead_name_char(c) || (c >= '0' && c <= '9');
}

bool TableStream::is_namestring_start(u8 c)
{
    return TableStream::is_lead_name_char(c) || (c == '^') || (c == '\\') || (c == (u8)Prefix::DualNamePrefix) || (c == (u8)Prefix::MultiNamePrefix);
}

ErrorOr<NameString> TableStream::read_name_string()
{
    auto value = TRY(NameString::from_table_stream(*this));
    return value;
}

ErrorOr<BlockHeader> TableStream::read_table_block_header()
{
    // DefBlockHeader := TableSignature TableLength
    //                   SpecCompliance CheckSum
    //                   OemID OemTableID OemRevision
    //                   CreatorID CreatorRevision
    // TableSignature   := DWordData
    // TableLength      := DWordData
    // SpecCompliance   := ByteData
    // CheckSum         := ByteData
    // OemID            := ByteData(6)::ZeroFilled
    // OemTableID       := ByteData(8)::ZeroFilled
    // OemRevision      := DWordData
    // CreatorID        := DWordData
    // CreatorRevision  := DWordData
    auto table_signature = TRY(read_value<u32>());
    auto table_length = TRY(read_value<u32>());
    auto spec_compliance = TRY(read_value<u8>());
    auto checksum = TRY(read_value<u8>());
    auto raw_oem_id = TRY(read_in_place<u8>(6));
    auto raw_oem_table_id = TRY(read_in_place<u8>(8));
    auto oem_revision = TRY(read_value<u32>());
    auto creator_id = TRY(read_value<u32>());
    auto creator_revision = TRY(read_value<u32>());

    AK::Array<u8, 6> oem_id;
    raw_oem_id.copy_to(oem_id);

    AK::Array<u8, 8> oem_table_id;
    raw_oem_table_id.copy_to(oem_table_id);

    return BlockHeader(table_signature, table_length, spec_compliance, checksum, oem_id, oem_table_id, oem_revision, creator_id, creator_revision);
}

ErrorOr<ByteBuffer> TableStream::read_string()
{
    size_t start = offset();
    u8 b = TRY(read_value<u8>());
    do {
        if (b == 0 || b > 0x7f) {
            return Error::from_string_literal("Invalid character in string!");
        }

        b = TRY(read_value<u8>());
    } while (b != 0);

    size_t size = offset() - start;
    TRY(seek(start));

    auto span = TRY(read_in_place<u8>(size - 1));
    // Skip over null byte so it doesn't end up in the buffer.
    TRY(seek(1, SeekMode::FromCurrentPosition));
    return ByteBuffer::copy(span);
}

ErrorOr<u16> TableStream::read_opcode()
{
    u16 opcode = TRY(read_value<u8>());
    if (opcode == static_cast<u16>(Prefix::ExtOpPrefix)) {
        opcode = (opcode << 8) | TRY(read_value<u8>());
    }
    return opcode;
}

ErrorOr<u32> TableStream::read_package_length()
{
    u8 initial = TRY(read_value<u8>());
    if ((initial & 0b11000000) == 0) {
        return initial;
    }

    i64 value = (initial & 0b1111);
    u8 additional_byte_count = initial >> 6;
    int bits = 4;
    for (auto i = 0; i < additional_byte_count; i++) {
        value = value | (TRY(read_value<u8>()) << bits);
        bits += 8;
    }

    return value;
}

ErrorOr<u16> TableStream::peek_opcode()
{
    auto previous = offset();
    u16 value = TRY(read_opcode());
    TRY(seek(previous));
    return value;
}

ErrorOr<u32> TableStream::peek_package_length()
{
    auto previous = offset();
    u32 value = TRY(read_package_length());
    TRY(seek(previous));
    return value;
}

ErrorOr<u8> TableStream::generate_checksum()
{
    auto previous = offset();
    u8 checksum = 0;

    TRY(seek(0));
    for (size_t i = 0; i < TRY(size()); i++) {
        checksum += TRY(read_value<u8>());
    }
    TRY(seek(previous));
    return checksum;
}

}
