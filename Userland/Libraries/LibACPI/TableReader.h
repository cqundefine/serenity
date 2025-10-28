/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include <AK/Array.h>
#include <AK/StringView.h>

namespace ACPI {

class TableReader {
public:
    TableReader(AK::StringView view)
        : m_view(view)
    {
    }

    // FIXME: Is this really the best place for these?
    static bool is_lead_name_char(u8 c);
    static bool is_name_char(u8 c);

    template<size_t N>
    AK::Array<u8, N> byte_array()
    {
        AK::Array<u8, N> result;
        for (size_t i = 0; i < N; i++) {
            u8 val = byte();
            result[i] = val;

            if (is_eof())
                break;
        }
        return result;
    }

    template<size_t N>
    AK::Array<u8, N> zero_terminated_byte_array()
    {
        AK::Array<u8, N> result;
        for (size_t i = 0; i < N; i++) {
            u8 val = byte();
            result[i] = val;

            if (is_eof())
                break;
        }
        return result;
    }

    void read_into(size_t len, AK::Vector<u8>& buffer);
    u8 peek();
    u8 byte();
    u16 word();
    u32 dword();
    u64 qword();
    AK::ErrorOr<AK::String> string();

    u16 opcode();
    i64 package_length();

    // FIXME: More descriptive name for this, or prefix the above methods
    //        with something to distinguish them.
    AK::StringView string_view(size_t size);
    AK::ErrorOr<AK::StringView> name_segment();
    u8 generate_checksum() const;

    void set_position(size_t pos) { m_position = pos; }

    bool is_eof() const { return m_position >= length(); }
    size_t position() const { return m_position; }
    size_t length() const { return m_view.length(); }

protected:
    AK::StringView m_view;
    size_t m_position { 0 };
};

}
