/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NameString.h"
#include "NodeData.h"
#include <AK/Array.h>
#include <AK/String.h>
#include <AK/Variant.h>

namespace ACPI {

enum class NodeType : u8 {
    Untyped,
    Device,
    Scope,
    Name,
    OperationRegion,
    Field,
    BufferField,
    Method,
    Processor,
    Alias,
    Mutex,
    ThermalZone
};

enum class FieldAccess : u8 {
    Any,
    Byte,
    Word,
    DWord,
    QWord,
    Buffer,
    Reserved0,
    Reserved1,
    Reserved2,
    Reserved3,
    Reserved4,
    Reserved5,
    Reserved6,
    Reserved7,
    Reserved8,
    Reserved9,
    ReservedA,
};

enum class FieldLock : u8 {
    NoLock,
    Lock
};

enum class FieldUpdateRule : u8 {
    Preserve,
    WriteAsOnes,
    WriteAsZeros,
    Reserved
};

enum class RegionSpace : u8 {
    SystemMemory,
    SystemIO,
    PciConfig,
    EmbeddedControl,
    SMBus,
    SystemCmos,
    PciBarTarget,
    IPMI,
    GPIO,
    GenericSerialBus,
    PCC,
    OemDefined
};

class Field {
public:
    Field(RefPtr<Node> const& operation_region, u8 flags, size_t offset, size_t size)
        : m_operation_region(operation_region)
        , m_flags(flags)
        , m_offset(offset)
        , m_size(size)
    {
    }

    RefPtr<Node> const& operation_region() const { return m_operation_region; }
    size_t offset() const { return m_offset; }
    size_t size() const { return m_size; }

    u8 flags() const { return m_flags; }

    FieldLock lock() const { return (m_flags & 0x10) ? FieldLock::Lock : FieldLock::NoLock; }
    FieldAccess access() const { return static_cast<FieldAccess>(m_flags & 0x0F); }
    FieldUpdateRule update_rule() const { return static_cast<FieldUpdateRule>((m_flags >> 5) & 0b11); }

protected:
    RefPtr<Node> m_operation_region;
    u8 m_flags;
    size_t m_offset;
    size_t m_size;
};

class Node : public RefCounted<Node> {
public:
    static ErrorOr<RefPtr<Node>> find_node(NameString const& path, RefPtr<Node> const& scope);

    ErrorOr<RefPtr<Node>> find_child(NameSegment name) const;
    ErrorOr<RefPtr<Node>> find_child(StringView name) const;

    // ErrorOr<RefPtr<Node>> find_neighbour(NameSegment name) const;
    // ErrorOr<RefPtr<Node>> find_neighbour(StringView name) const;

    ErrorOr<void> insert_child(NameSegment name, RefPtr<Node> const& node);
    ErrorOr<void> insert_child(StringView name, RefPtr<Node> const& node);
    // ErrorOr<void> insert_child_recursive(NameString path, RefPtr<Node> const& node);

    NameSegment name() const { return m_name; }

    RefPtr<Node> parent() const { return m_parent; }
    RefPtr<Node> child() const { return m_child; }
    RefPtr<Node> neighbour() const { return m_neighbour; }
    virtual void write_description(StringBuilder& builder) { builder.append(kind()); }

    virtual NodeType type() const { return NodeType::Untyped; }
    virtual StringView kind() const { return "Node"sv; }
    virtual ~Node() { }

protected:
    Node()
        : m_parent(nullptr)
        , m_child(nullptr)
        , m_neighbour(nullptr)
    {
    }
    Node(Node* parent)
        : m_parent(parent)
        , m_child(nullptr)
        , m_neighbour(nullptr)
    {
    }

    NameSegment m_name;
    RefPtr<Node> m_parent { nullptr };
    RefPtr<Node> m_child { nullptr };
    RefPtr<Node> m_neighbour { nullptr };
};

class DeviceNode : public Node {
public:
    DeviceNode()
        : Node(nullptr)
    {
    }
    ~DeviceNode() = default;

    NodeType type() const override { return NodeType::Device; }
    StringView kind() const override { return "Device"sv; }
};

class ScopeNode : public Node {
public:
    ScopeNode()
        : Node(nullptr)
    {
    }
    ~ScopeNode() = default;

    NodeType type() const override { return NodeType::Scope; }
    StringView kind() const override { return "Scope"sv; }
};

class NameNode : public Node {
public:
    NameNode(NodeData value)
        : Node(nullptr)
        , m_data(move(value))
    {
    }
    ~NameNode() = default;

    NodeData& data() { return m_data; }
    void write_description(StringBuilder& builder) override;
    NodeType type() const override { return NodeType::Name; }
    StringView kind() const override { return "Name"sv; }

protected:
    NodeData m_data;
};

class AliasNode : public Node {
public:
    AliasNode(NameString destination)
        : Node(nullptr)
        , m_destination_namestring(destination.clone(m_destination))
    {
    }

    void write_description(StringBuilder& builder) override;
    NodeType type() const override { return NodeType::Alias; }
    StringView kind() const override { return "Alias"sv; }

    NameString destination();

protected:
    ByteBuffer m_destination;
    NameString m_destination_namestring;
};

class OperationRegionNode : public Node {
public:
    OperationRegionNode(u8 region_space, i64 region_offset, i64 region_length)
        : m_space_raw(region_space)
        , m_offset(region_offset)
        , m_length(region_length)
    {
    }
    ~OperationRegionNode() = default;

    RegionSpace space() const
    {
        if (m_space_raw >= 0x80) {
            return RegionSpace::OemDefined;
        }
        return static_cast<RegionSpace>(m_space_raw);
    }
    u8 space_raw() const { return m_space_raw; }

    i64 offset() const { return m_offset; }
    i64 length() const { return m_length; }

    NodeType type() const override { return NodeType::OperationRegion; }
    StringView kind() const override { return "Op. Region"sv; }

protected:
    u8 m_space_raw;
    i64 m_offset;
    i64 m_length;
};

class FieldNode : public Node {
public:
    FieldNode(Field field)
        : m_field(move(field))
    {
    }
    ~FieldNode() = default;
    NodeType type() const override { return NodeType::Field; }
    StringView kind() const override { return "Field"sv; }

    Field field() const { return m_field; }

protected:
    Field m_field;
};

class BufferFieldNode : public Node {
public:
    BufferFieldNode(Buffer* buffer_ptr, int bit_offset, int bit_size)
        : m_buffer_ptr(buffer_ptr)
        , m_bit_offset(bit_offset)
        , m_bit_size(bit_size)
    {
    }
    ~BufferFieldNode() = default;

    virtual void write_description(StringBuilder& builder) override;
    NodeType type() const override { return NodeType::BufferField; }
    StringView kind() const override { return "BufferField"sv; }

protected:
    Buffer* m_buffer_ptr;
    int m_bit_offset;
    int m_bit_size;
};

class MethodNode : public Node {
public:
    MethodNode(ssize_t table_index, size_t start, size_t end, u8 flags)
        : m_table_index(table_index)
        , m_start(start)
        , m_end(end)
        , m_flags(flags)
    {
    }
    ~MethodNode() = default;

    void write_description(StringBuilder& builder) override;
    NodeType type() const override { return NodeType::Method; }
    StringView kind() const override { return "Method"sv; }

    ssize_t table_index() const { return m_table_index; }
    size_t start() const { return m_start; }
    size_t end() const { return m_end; }
    u8 flags() const { return m_flags; }
    u8 arguments() const { return m_flags & 7; }

protected:
    ssize_t m_table_index { 0 };
    size_t m_start { 0 };
    size_t m_end { 0 };
    u8 m_flags { 0 };
};

class ProcessorNode : public Node {
public:
    ProcessorNode(u32 address, u8 id, u8 block_length)
        : m_address(address)
        , m_id(id)
        , m_block_length(block_length)
    {
    }

    NodeType type() const override { return NodeType::Processor; }
    StringView kind() const override { return "Processor (Depr.)"sv; }

protected:
    u32 m_address { 0 };
    u8 m_id { 0 };
    u8 m_block_length { 0 };
};

class ThermalZoneNode : public Node {
public:
    ThermalZoneNode() { }

    NodeType type() const override { return NodeType::ThermalZone; }
    StringView kind() const override { return "ThermalZone"sv; }
};

class MutexNode : public Node {
public:
    MutexNode(u8 sync_flags)
        : m_sync_flags(sync_flags)
    {
    }

    NodeType type() const override { return NodeType::Mutex; }
    StringView kind() const override { return "Mutex"sv; }
    u8 sync_flags() const { return m_sync_flags; }

protected:
    u8 m_sync_flags { 0 };
};

}
