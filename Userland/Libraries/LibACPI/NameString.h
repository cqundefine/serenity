/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Result.h>
#include <AK/String.h>
#include <AK/StringView.h>

namespace ACPI {

class TableStream;
class NameSegment {
public:
    NameSegment()
        : m_data({ '_', '_', '_', '_' })
    {
    }

    static ErrorOr<NameSegment> from_string_view(StringView view);
    static ErrorOr<NameSegment> from_readonly_bytes(ReadonlyBytes span);

    friend bool operator==(NameSegment const& l, NameSegment const& r) { return l.m_data == r.m_data; }
    friend bool operator!=(NameSegment const& l, NameSegment const& r) { return l.m_data != r.m_data; }

    StringView to_string_view() const;

protected:
    friend class NameString;
    NameSegment(Array<char, 4> value);
    Array<char, 4> m_data;
};

class NameString {
public:
    enum class Type : u8 {
        Absolute,
        Relative
    };

    static ErrorOr<NameString> from_string(StringView const& str);
    static ErrorOr<NameString> from_table_stream(TableStream& stream);

    ErrorOr<NameString> dirname() const;
    ErrorOr<NameSegment> basename() const;

    ErrorOr<String> to_string() const;
    ErrorOr<NameSegment> segment(size_t index) const;
    Type type() const { return m_type; }
    size_t depth() const { return m_depth; }
    size_t count() const { return m_count; }

    // Clone namestring and its underlying data
    // into a heap allocated ByteBuffer.
    NameString clone(ByteBuffer& buf);

protected:
    NameString(Type type, size_t depth)
        : m_type(type)
        , m_depth(depth)
        , m_count(0)
        , m_additional_unit_bytes(0)
    {
    }
    NameString(Type type, size_t depth, int additional_unit_bytes, size_t count, ReadonlyBytes sequence)
        : m_type(type)
        , m_depth(depth)
        , m_count(count)
        , m_additional_unit_bytes(additional_unit_bytes)
        , m_name_sequence(sequence)
    {
    }

    Type m_type { Type::Relative };
    size_t m_depth { 0 };
    size_t m_count { 0 };

    int m_additional_unit_bytes { 0 };
    ReadonlyBytes m_name_sequence;
};

}
