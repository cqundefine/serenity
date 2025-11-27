/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NameString.h"
#include "Definitions.h"
#include "TableStream.h"

namespace ACPI {

NameSegment::NameSegment(Array<char, 4> value)
    : m_data(value)
{
}

ErrorOr<NameSegment> NameSegment::from_string_view(StringView view)
{
    if (!TableStream::is_lead_name_char(view[0]) || !TableStream::is_name_char(view[1]) || !TableStream::is_name_char(view[2]) || !TableStream::is_name_char(view[3])) {
        warnln("[LibACPI] Invalid character found inside name segment: '{}'", view);
        return Error::from_string_literal("Invalid character found inside name segment.");
    }

    return NameSegment({ (char)view[0], (char)view[1], (char)view[2], (char)view[3] });
}

ErrorOr<NameSegment> NameSegment::from_readonly_bytes(ReadonlyBytes span)
{
    if (!TableStream::is_lead_name_char(span[0]) || !TableStream::is_name_char(span[1]) || !TableStream::is_name_char(span[2]) || !TableStream::is_name_char(span[3])) {
        warnln("[LibACPI] Invalid character found inside name segment: '{}'", span);
        return Error::from_string_literal("Invalid character found inside name segment.");
    }

    return NameSegment({ (char)span[0], (char)span[1], (char)span[2], (char)span[3] });
}

StringView NameSegment::to_string_view() const
{
    StringView view(m_data.data(), 4);
    return view;
}

ErrorOr<NameString> NameString::from_table_stream(TableStream& stream)
{
    Type type = Type::Relative;
    size_t depth = 0;
    size_t count = 0;
    u8 initial = 0;

    // Figure out path type.
    initial = TRY(stream.peek_value<u8>());
    if (initial == '\\') {
        // dbgln("[LibACPI] NameString::from_reader: Absolute path.");
        type = Type::Absolute;
        TRY(stream.read_value<u8>());
    } else if (initial == '^') {
        do {
            depth++;
            TRY(stream.read_value<u8>());
            initial = TRY(stream.peek_value<u8>());
        } while (initial == '^' && !stream.is_eof());

        // dbgln("[LibACPI] NameString::from_reader: Relative path, depth={}.", depth);
    } else {
        // dbgln("[LibACPI] NameString::from_reader: Relative path, zero depth.");
    }

    // Figure out how many segments are in the path.
    // NamePath := NameSeg | DualNamePath | MultiNamePath | NullName
    initial = TRY(stream.peek_value<u8>());
    switch (initial) {
    case 0:
        // dbgln("[LibACPI] NameString::from_reader: Null name path.");
        TRY(stream.read_value<u8>());
        return NameString(type, depth);

    case (u8)Prefix::MultiNamePrefix:
        TRY(stream.read_value<u8>());
        count = TRY(stream.read_value<u8>());

        if (count == 0) {
            return Error::from_string_literal("Multiname path must have at least 1 item.");
        }
        break;

    case (u8)Prefix::DualNamePrefix:
        TRY(stream.read_value<u8>());
        count = 2;
        break;

    default:
        if (TableStream::is_lead_name_char(initial)) {
            count = 1;
            break;
        } else {
            warnln("[LibACPI] Invalid character: '{:X}' at offset {:X}", (char)initial, stream.offset());
            return Error::from_string_literal("Invalid character found.");
        }
    }

    // Validate items inside remaining path..
    auto span = TRY(stream.read_in_place<u8>(count * 4));
    for (size_t i = 0; i < count; i++) {
        auto item = span.slice(i * 4, 4);
        if (!TableStream::is_lead_name_char(item[0]) || !TableStream::is_name_char(item[1]) || !TableStream::is_name_char(item[2]) || !TableStream::is_name_char(item[3])) {
            warnln("[LibACPI] Invalid character found inside name segment: '{:X}'", item);
            return Error::from_string_literal("Invalid character found inside name segment.");
        }
    }
    return NameString(type, depth, 0, count, span);
}

ErrorOr<NameString> NameString::from_string(StringView const& str)
{
    Type type = Type::Relative;
    size_t depth = 0;
    size_t count = 0;
    size_t i = 0;

    // Header parsing works the same as the bytecode variant.
    u8 initial = str[i++];
    if (initial == '\\') {
        type = NameString::Type::Absolute;
        depth = 0;

        if (i >= str.length()) {
            // Completely empty, no need to store reference to any string view.
            return NameString(type, depth);
        }

        initial = str[i++];
    } else if (initial == '^') {
        // PrefixPath := Nothing | <'^' prefixpath>
        type = NameString::Type::Relative;
        while ((i > str.length()) && initial == '^') {
            depth++;
            initial = str[i++];
        }
    }

    if (i >= str.length()) {
        // Also completely empty, see above.
        return NameString(type, depth);
    }

    // Validate segments, alongside separators.
    size_t segment_start = i;
    for (; i < str.length(); i += 5) {
        if (!TableStream::is_lead_name_char(str[i]) || !TableStream::is_name_char(str[i + 1]) || !TableStream::is_name_char(str[i + 2]) || !TableStream::is_name_char(str[i + 3])) {
            warnln("[LibACPI] Invalid character found inside name segment: '{}'", str);
            return Error::from_string_literal("Invalid character found inside name segment.");
        }

        if (str[i + 4] != '.' && str[i + 4] != 0) {
            return Error::from_string_literal("Invalid name segment continuation.");
        }
        count++;
    }

    // Eugh.
    auto view = str.substring_view(segment_start, count * 5);
    return NameString(type, depth, 1, count, view.bytes());
}

ErrorOr<NameSegment> NameString::segment(std::size_t index) const
{
    if (index >= m_count) {
        return Error::from_string_literal("Segment index out of bounds!");
    }

    return NameSegment::from_readonly_bytes(m_name_sequence.slice(index * (4 + m_additional_unit_bytes), 4));
}

ErrorOr<String> NameString::to_string() const
{
    StringBuilder builder;
    if (m_type == NameString::Type::Absolute) {
        builder.append('\\');
    } else if (m_depth > 0) {
        builder.append_repeated('^', m_depth);
    }

    for (size_t i = 0; i < m_count; i++) {
        builder.append(TRY(segment(i)).to_string_view());
        if (i != m_count - 1) {
            builder.append('.');
        }
    }

    return builder.to_string();
}

ErrorOr<NameString> NameString::dirname() const
{
    if (m_count == 0) {
        return Error::from_string_literal("NullNameString has no dir name!");
    }
    return NameString(m_type, m_depth, m_additional_unit_bytes, m_count - 1, m_name_sequence);
}

ErrorOr<NameSegment> NameString::basename() const
{
    if (m_count == 0) {
        return Error::from_string_literal("NullNameString has no base name!");
    }
    return segment(m_count - 1);
}

NameString NameString::clone(ByteBuffer& buf)
{
    buf.resize(m_name_sequence.size());
    m_name_sequence.copy_to(buf);
    return NameString(m_type, m_depth, m_additional_unit_bytes, m_count, buf.bytes());
}

}
