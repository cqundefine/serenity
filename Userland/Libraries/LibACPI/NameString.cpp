/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NameString.h"
#include "Definitions.h"

namespace ACPI {

NameSegment::NameSegment(AK::Array<char, 4> value)
    : m_data(value)
{
}

ErrorOr<NameSegment> NameSegment::from_string_view(AK::StringView view)
{
    if (!TableReader::is_lead_name_char(view[0]) || !TableReader::is_name_char(view[1]) || !TableReader::is_name_char(view[2]) || !TableReader::is_name_char(view[3])) {
        warnln("[LibACPI] Invalid character found inside name segment: '{}'", view);
        return Error::from_string_literal("Invalid character found inside name segment.");
    }

    return NameSegment({ view[0], view[1], view[2], view[3] });
}

AK::StringView NameSegment::to_string_view()
{
    AK::StringView view(m_data.data(), 4);
    return view;
}

AK::ErrorOr<NameString> NameString::from_reader(TableReader& reader)
{
    Type type = Type::Relative;
    size_t depth = 0;
    size_t count = 0;
    u8 initial = 0;

    // Figure out path type.
    initial = reader.peek();
    if (initial == '\\') {
        dbgln("[LibACPI] NameString::from_reader: Absolute path.");
        type = Type::Absolute;
        reader.byte();
    } else if (initial == '^') {
        do {
            depth++;
            reader.byte();
            initial = reader.peek();
        } while (initial == '^' && !reader.is_eof());

        reader.byte();
        dbgln("[LibACPI] NameString::from_reader: Relative path, depth={}.", depth);
    } else {
        dbgln("[LibACPI] NameString::from_reader: Relative path, zero depth.");
    }

    // Figure out how many segments are in the path.
    // NamePath := NameSeg | DualNamePath | MultiNamePath | NullName
    initial = reader.peek();
    switch (initial) {
    case 0:
        // dbgln("[LibACPI] NameString::from_reader: Null name path.");
        reader.byte();
        return NameString(type, depth);

    case (u8)Prefix::MultiNamePrefix:
        reader.byte();

        count = reader.byte();
        if (count == 0) {
            return Error::from_string_literal("Multiname path must have at least 1 item.");
        }
        break;

    case (u8)Prefix::DualNamePrefix:
        count = 2;
        reader.byte();
        break;

    default:
        if (TableReader::is_lead_name_char(initial)) {
            count = 1;
            break;
        } else {
            warnln("[LibACPI] Invalid character: '{:X}' at position {:X}", (char)initial, reader.position());
            return Error::from_string_literal("Invalid character found.");
        }
    }

    // Validate items inside remaining path..
    auto view = reader.string_view(count * 4);
    for (size_t i = 0; i < count; i++) {
        auto item = view.substring_view(i * 4, 4);
        if (!TableReader::is_lead_name_char(item[0]) || !TableReader::is_name_char(item[1]) || !TableReader::is_name_char(item[2]) || !TableReader::is_name_char(item[3])) {
            warnln("[LibACPI] Invalid character found inside name segment: '{:X}'", item);
            return Error::from_string_literal("Invalid character found inside name segment.");
        }
    }
    return NameString(type, depth, 0, count, view);
}

AK::ErrorOr<NameString> NameString::from_string(AK::StringView const& str)
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
        if (!TableReader::is_lead_name_char(str[i]) || !TableReader::is_name_char(str[i + 1]) || !TableReader::is_name_char(str[i + 2]) || !TableReader::is_name_char(str[i + 3])) {
            warnln("[LibACPI] Invalid character found inside name segment: '{}'", str);
            return Error::from_string_literal("Invalid character found inside name segment.");
        }

        if (str[i + 4] != '.' && str[i + 4] != 0) {
            return Error::from_string_literal("Invalid name segment continuation.");
        }
        count++;
    }

    // Eugh.
    return NameString(type, depth, 1, count, str.substring_view(segment_start, count * 5));
}

AK::ErrorOr<NameSegment> NameString::segment(std::size_t index) const
{
    if (index >= m_count) {
        return Error::from_string_literal("Segment index out of bounds!");
    }

    return NameSegment::from_string_view(m_name_sequence.substring_view(index * (4 + m_additional_unit_bytes), 4));
}

AK::ErrorOr<AK::String> NameString::to_string() const
{
    AK::StringBuilder builder;
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

AK::ErrorOr<NameString> NameString::dirname() const
{
    if (m_count == 0) {
        return Error::from_string_literal("NullNameString has no dir name!");
    }
    return NameString(m_type, m_depth, m_additional_unit_bytes, m_count - 1, m_name_sequence);
}

AK::ErrorOr<NameSegment> NameString::basename() const
{
    if (m_count == 0) {
        return Error::from_string_literal("NullNameString has no base name!");
    }
    return segment(m_count - 1);
}

}
