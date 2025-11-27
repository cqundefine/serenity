/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NameString.h"
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Variant.h>

namespace ACPI {

class Buffer {
public:
    Buffer(ByteBuffer buffer)
        : m_buffer(move(buffer))
    {
    }

    ByteBuffer& buffer() { return m_buffer; }

protected:
    ByteBuffer m_buffer;
};

class Node;
class NodeData {
public:
    enum class Type : u8 {
        None,
        Integer,
        String,
        Buffer,
        Package,
        NameString
    };

    NodeData()
        : m_type(Type::None)
        , m_data(0)
    {
    }
    NodeData(i64 value)
        : m_type(Type::Integer)
        , m_data(value)
    {
    }
    NodeData(Buffer const& value)
        : m_type(Type::Buffer)
        , m_data(value)
    {
    }
    NodeData(ByteBuffer const& value)
        : m_type(Type::String)
        , m_data(value)
    {
    }
    NodeData(Vector<NodeData> const& value)
        : m_type(Type::Package)
        , m_data(value)
    {
    }
    NodeData(NameString const& value)
        : m_type(Type::NameString)
        , m_data(value)
    {
    }

    Type type() const { return m_type; }

    ErrorOr<NameString> as_namestring();
    ErrorOr<i64> as_integer();

    ErrorOr<StringView> as_string_view();
    ErrorOr<ByteBuffer> as_string_raw();

    ErrorOr<Buffer*> as_buffer_ptr();

protected:
    Type m_type { Type::None };
    Variant<NameString, ByteBuffer, i64, Buffer, Vector<NodeData>> m_data;
};

}
