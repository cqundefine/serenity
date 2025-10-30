/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include <AK/String.h>
#include <AK/Types.h>
#include <AK/Variant.h>

namespace ACPI {

class Buffer {
public:
    Buffer(Vector<u8> data)
        : m_raw(move(data))
    {
    }

protected:
    Vector<u8> m_raw;
};

class Node;
class NodeData {
public:
    enum class Type : u8 {
        None,
        Byte,
        Word,
        DWord,
        QWord,
        String,
        Buffer,
        Package
    };

    NodeData()
        : m_type(Type::None)
        , m_data(0)
    {
    }
    NodeData(i8 value)
        : m_type(Type::Byte)
        , m_data(value)
    {
    }
    NodeData(i16 value)
        : m_type(Type::Word)
        , m_data(value)
    {
    }
    NodeData(i32 value)
        : m_type(Type::DWord)
        , m_data(value)
    {
    }
    NodeData(i64 value)
        : m_type(Type::QWord)
        , m_data(value)
    {
    }
    NodeData(Buffer const& value)
        : m_type(Type::Buffer)
        , m_data(value)
    {
    }
    NodeData(String const& value)
        : m_type(Type::String)
        , m_data(value)
    {
    }
    NodeData(Vector<NodeData> const& value)
        : m_type(Type::Package)
        , m_data(value)
    {
    }

    Type type() const { return m_type; }

    ErrorOr<i64> as_integer();

protected:
    Type m_type { Type::None };
    Variant<String, i8, i16, i32, i64, Buffer, Vector<NodeData>> m_data;
};

}
