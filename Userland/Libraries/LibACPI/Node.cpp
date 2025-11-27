/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Node.h"

namespace ACPI {

ErrorOr<RefPtr<Node>> Node::find_node(NameString const& path, RefPtr<Node> const& scope)
{
    // dbgln("[LibACPI] Node::find_node: Request to find path '{}' in node {}.", TRY(path.to_string()), scope->name().to_string_view());

    auto root = scope;

    bool search = path.type() == NameString::Type::Relative && path.count() == 1 && path.depth() == 0;
    if (path.type() == NameString::Type::Relative && path.depth() > 0) {
        // Move up through the tree.
        for (size_t i = 0; i < path.depth(); i++) {
            root = root->parent();
            if (root.is_null()) {
                return Error::from_string_literal("Can't go higher than root!");
            }
        }
    } else if (path.type() == NameString::Type::Absolute) {
        // Find root of table.
        while (!root->parent().is_null()) {
            root = root->parent();
        }
    }

    if (search) {
        // Path is a single name segment, is relative and has no depth.
        // Crawl upwards from the current scope and try finding a node with the given name.
        auto node_name = TRY(path.segment(0));
        while (!root.is_null()) {
            auto found = root->find_child(node_name);
            if (found.is_error()) {
                root = root->parent();
                continue;
            }

            return found.release_value();
        }

        warnln("[LibACPI] Node::find_node: Could not find node with name {}!", node_name.to_string_view());
        return Error::from_string_literal("Node not found!");
    }

    // Search crawls down from root via name segments.
    for (size_t i = 0; i < path.count(); i++) {
        auto node_name = TRY(path.segment(i));
        auto found = root->find_child(node_name);
        if (found.is_error()) {
            // FIXME: Warn without allocating any string.
            warnln("[LibACPI] Node::find_node: Could not find node at path {}!", path.to_string());
            return found.release_error();
        }

        root = found.release_value();
    }
    return root;
}

ErrorOr<RefPtr<Node>> Node::find_child(NameSegment name) const
{
    auto child = m_child;
    while (child && (child->name() != name)) {
        child = child->neighbour();
    }

    if (child.is_null()) {
        warnln("[LibACPI] Node::find_child: Child {} does not exist relative to node {}!", name.to_string_view(), m_name.to_string_view());
        return Error::from_string_literal("Child does not exist.");
    }
    return child;
}

// ErrorOr<RefPtr<Node>> Node::find_neighbour(NameSegment name) const
//{
//     auto child = m_neighbour;
//     while (child && (child->name() != name))
//     {
//         child = child->neighbour();
//     }
//
//     if (child.is_null())
//         return Error::from_string_literal("Neighbour does not exist.");
//     return child;
// }

ErrorOr<RefPtr<Node>> Node::find_child(StringView name) const
{
    if (name.length() != 4)
        return Error::from_string_literal("Name must have a lenth of 4.");

    auto broken_name = TRY(NameSegment::from_string_view(name));
    return find_child(broken_name);
}

// ErrorOr<RefPtr<Node>> Node::find_neighbour(StringView name) const
//{
//     if (name.length() != 4)
//         return Error::from_string_literal("Name must have a lenth of 4.");
//
//     auto broken_name = TRY(NameSegment::from_string_view(name));
//     return find_neighbour(broken_name);
// }

ErrorOr<void> Node::insert_child(NameSegment name, RefPtr<Node> const& node)
{
    /// FIXME: Duplicate name detection.
    node->m_name = name;
    node->m_parent = this;
    node->m_neighbour = m_child;
    m_child = node;
    return {};
}

ErrorOr<void> Node::insert_child(StringView name, RefPtr<Node> const& node)
{
    if (name.length() != 4)
        return Error::from_string_literal("Name must have a lenth of 4.");

    auto broken_name = TRY(NameSegment::from_string_view(name));
    return insert_child(broken_name, node);
}

// FIXME: Split off into separate <x>Node files.
static StringView node_data_type_to_string_view(NodeData::Type type)
{
    switch (type) {
    case NodeData::Type::None:
        return "None"sv;
    case NodeData::Type::Integer:
        return "Integer"sv;
    case NodeData::Type::String:
        return "String"sv;
    case NodeData::Type::Buffer:
        return "Buffer"sv;
    case NodeData::Type::Package:
        return "Package"sv;
    default:
        return "N/A"sv;
    }
}

void NameNode::write_description(StringBuilder& builder)
{
    builder.append(node_data_type_to_string_view(m_data.type()));
    switch (m_data.type()) {
    case NodeData::Type::Integer: {
        auto value = m_data.as_integer().release_value_but_fixme_should_propagate_errors();
        builder.appendff("(Decimal: {}, Hexadecimal: {:X})", value, value);
        break;
    }
    case NodeData::Type::String: {
        // FIXME: Currently the TableStream doesn't include the null byte in the ByteBuffer,
        //        to ensure it doesn't get included in a case like this.
        //        Figure out if this is correct.
        auto value = m_data.as_string_raw().release_value_but_fixme_should_propagate_errors();
        auto view = StringView(value);
        builder.appendff("(Buffer: '{}')", view);

        break;
    }
    case NodeData::Type::Buffer: {
        auto* value = m_data.as_buffer_ptr().release_value_but_fixme_should_propagate_errors();
        auto buffer = value->buffer();

        builder.appendff("(Capacity: {}, Size: {})", buffer.capacity(), buffer.size());
        break;
    }
    default:
        break;
    }
}

void MethodNode::write_description(StringBuilder& builder)
{
    builder.appendff("Method(Table: {}, Args: {}, Start: {}, End: {}, Flags: {})", table_index(), arguments(), start(), end(), flags());
}

void BufferFieldNode::write_description(StringBuilder& builder)
{
    builder.appendff("BufferField(Offset: {}, Size: {}, ptr: {})", m_bit_offset, m_bit_size, m_buffer_ptr);
}

void AliasNode::write_description(StringBuilder& builder)
{
    builder.appendff("Alias(Destionation: {})", m_destination_namestring.to_string().release_value_but_fixme_should_propagate_errors());
}
}
