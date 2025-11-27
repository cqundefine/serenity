/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "BlockHeader.h"
#include "NameString.h"
#include <AK/Array.h>
#include <AK/MemoryStream.h>
#include <AK/StringView.h>

namespace ACPI {

class TableStream : public FixedMemoryStream {
public:
    // FIXME: Is this really the best place for these?
    static bool is_namestring_start(u8 c);
    static bool is_lead_name_char(u8 c);
    static bool is_name_char(u8 c);

    explicit TableStream(Bytes bytes, Mode mode = Mode::ReadWrite)
        : FixedMemoryStream(bytes, mode)
    {
    }
    explicit TableStream(ReadonlyBytes bytes)
        : FixedMemoryStream(bytes)
    {
    }

    ErrorOr<NameString> read_name_string();

    ErrorOr<BlockHeader> read_table_block_header();
    ErrorOr<ByteBuffer> read_string();
    ErrorOr<u16> read_opcode();
    ErrorOr<u32> read_package_length();

    ErrorOr<u16> peek_opcode();
    ErrorOr<u32> peek_package_length();

    ErrorOr<u8> generate_checksum();
};

}
