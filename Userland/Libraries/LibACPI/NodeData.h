/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 * Copyright (c) 2026, Undefine <undefine@undefine.pl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NameString.h"
#include <AK/ByteBuffer.h>
#include <AK/Types.h>
#include <AK/Variant.h>
#include <AK/Vector.h>

namespace ACPI {

class Node;
class NodeData;

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

class Package {
public:
    Package(Vector<NodeData> package)
        : m_package(move(package))
    {
    }

    // FIXME: Remove this ugly hack, it's needed for now for Variant compatibility in Kernel land
    Package(Package const& other)
        : m_package(other.m_package.clone().release_value_but_fixme_should_propagate_errors())
    {
    }

    Vector<NodeData>& package() { return m_package; }
    Vector<NodeData> const& package() const { return m_package; }

protected:
    Vector<NodeData> m_package;
};

// TODO: This should get refactored with templates
// TODO: This needs to respect the 32-bit version == 1 / 64-bit version >= 2
class NodeData {
public:
    enum class Type : u8 {
        None,
        Integer,
        String,
        Buffer,
        Package,
        NameString,
        Reference,
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
    NodeData(Vector<NodeData> value)
        : m_type(Type::Package)
        , m_data(move(value))
    {
    }
    NodeData(NameString const& value)
        : m_type(Type::NameString)
        , m_data(value)
    {
    }
    NodeData(NodeData* value)
        : m_type(Type::Reference)
        , m_data(value)
    {
    }

    Type type() const { return m_type; }

    ErrorOr<NameString> as_namestring();
    ErrorOr<i64> as_integer();

    ErrorOr<StringView> as_string_view();
    ErrorOr<ByteBuffer> as_string_raw();

    ErrorOr<Buffer*> as_buffer_ptr();
    ErrorOr<Package*> as_package_ptr();

    ErrorOr<NodeData*> as_reference();

protected:
    NodeData* maybe_resolve_reference();

    Type m_type { Type::None };
    Variant<NameString, ByteBuffer, i64, Buffer, Package, NodeData*> m_data;
};

}
