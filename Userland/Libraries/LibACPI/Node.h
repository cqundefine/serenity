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

    virtual void write_description(StringBuilder& builder) { builder.append(type()); }
    virtual StringView type() const { return "Node"sv; }
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

    StringView type() const override { return "Device"sv; }
};

class ScopeNode : public Node {
public:
    ScopeNode()
        : Node(nullptr)
    {
    }
    ~ScopeNode() = default;

    StringView type() const override { return "Scope"sv; }
};

class NameNode : public Node {
public:
    NameNode(NodeData value)
        : Node(nullptr)
        , m_data(move(value))
    {
    }
    ~NameNode() = default;

    void write_description(StringBuilder& builder) override;
    StringView type() const override { return "Name"sv; }

protected:
    NodeData m_data;
};

class OperationRegionNode : public Node {
public:
    OperationRegionNode(u8 region_space, i64 region_offset, i64 region_length)
        : m_space(region_space)
        , m_offset(region_offset)
        , m_length(region_length)
    {
    }
    ~OperationRegionNode() = default;
    StringView type() const override { return "Op. Region"sv; }

protected:
    u8 m_space;
    i64 m_offset;
    i64 m_length;
};

class Field {
public:
    Field(RefPtr<Node> const& operation_region, u8 flags)
        : m_operation_region(operation_region)
        , m_flags(flags)
    {
    }
    RefPtr<Node> const& operation_region() const { return m_operation_region; }
    u8 flags() const { return m_flags; }

protected:
    RefPtr<Node> m_operation_region;
    u8 m_flags;
};

class FieldNode : public Node {
public:
    FieldNode(Field field)
        : m_field(move(field))
    {
    }
    ~FieldNode() = default;
    StringView type() const override { return "Field"sv; }

    Field field() const { return m_field; }

protected:
    Field m_field;
};

class MethodNode : public Node {
public:
    MethodNode(size_t start, size_t end, u8 flags)
        : m_start(start)
        , m_end(end)
        , m_flags(flags)
    {
    }
    ~MethodNode() = default;

    void write_description(StringBuilder& builder) override;
    StringView type() const override { return "Method"sv; }

    size_t start() const { return m_start; }
    size_t end() const { return m_end; }
    u8 flags() const { return m_flags; }
    u8 arguments() const { return m_flags & 7; }

protected:
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
    StringView type() const override { return "Processor (Depr.)"sv; }

protected:
    u32 m_address { 0 };
    u8 m_id { 0 };
    u8 m_block_length { 0 };
};

}
