/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Node.h"

namespace ACPI {

AK::ErrorOr<AK::RefPtr<Node>> Node::find_node(NameString const& path, AK::RefPtr<Node> const& scope)
{
    auto target = scope;
    if (path.type() == NameString::Type::Relative && path.depth() > 0) {
        // Move up through the tree.
        for (size_t i = 0; i < path.depth(); i++) {
            target = target->parent();
            if (target.is_null()) {
                return Error::from_string_literal("Can't go higher than root!");
            }
        }
    } else if (path.type() == NameString::Type::Absolute) {
        // Find root of table.
        while (!target->parent().is_null()) {
            target = target->parent();
        }
    }

    for (size_t i = 0; i < path.count(); i++) {
        auto name = TRY(path.segment(i));
        auto found = TRY(target->find_child(name));
        target = found;
        if (target.is_null()) {
            return Error::from_string_literal("Target node not found!");
        }
    }

    return target;
}

ErrorOr<AK::RefPtr<Node>> Node::find_child(NameSegment name) const
{
    auto child = m_child;
    while (child && (child->name() != name)) {
        child = child->neighbour();
    }

    if (child.is_null()) {
        return Error::from_string_literal("Child does not exist.");
    }
    return child;
}

// ErrorOr<AK::RefPtr<Node>> Node::find_neighbour(NameSegment name) const
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

ErrorOr<AK::RefPtr<Node>> Node::find_child(AK::StringView name) const
{
    if (name.length() != 4)
        return Error::from_string_literal("Name must have a lenth of 4.");

    auto broken_name = TRY(NameSegment::from_string_view(name));
    return find_child(broken_name);
}

// ErrorOr<AK::RefPtr<Node>> Node::find_neighbour(AK::StringView name) const
//{
//     if (name.length() != 4)
//         return Error::from_string_literal("Name must have a lenth of 4.");
//
//     auto broken_name = TRY(NameSegment::from_string_view(name));
//     return find_neighbour(broken_name);
// }

ErrorOr<void> Node::insert_child(NameSegment name, AK::RefPtr<Node> const& node)
{
    /// FIXME: Duplicate name detection.
    node->m_name = name;
    node->m_parent = this;
    node->m_neighbour = m_child;
    m_child = node;
    return {};
}

ErrorOr<void> Node::insert_child(AK::StringView name, AK::RefPtr<Node> const& node)
{
    if (name.length() != 4)
        return Error::from_string_literal("Name must have a lenth of 4.");

    auto broken_name = TRY(NameSegment::from_string_view(name));
    return insert_child(broken_name, node);
}

// static ErrorOr<AK::RefPtr<Node>> get_node_at_dirname(AK::RefPtr<Node> const& root, NameString path)
//{
//     // Move upwards towards root.
//     AK::RefPtr<Node> target = root;
//     if (path.type() == NameString::Type::Relative && path.depth() > 0)
//     {
//         for (size_t i = 0; i < path.depth(); i++)
//         {
//             target = target->parent();
//             if (target.is_null())
//             {
//                 return Error::from_string_literal("Can't go higher than root.");
//             }
//         }
//     } else if (path.type() == NameString::Type::Absolute)
//     {
//         while (!target->parent().is_null())
//         {
//             target = target->parent();
//         }
//     }
//
//     if (path.count() != 0)
//     {
//         auto dirname = TRY(path.dirname());
//         warnln("Attempting to find node at path {} in node with name {}", path.to_string(), root->name().to_string_view());
//         // Move down, hopefully towards target node.
//         for (size_t idx = 0; idx < dirname.count(); idx++)
//         {
//             auto name = TRY(dirname.segment(idx));
//             auto found = TRY(target->find_child(name));
//             target = found;
//         }
//     }
//
//     return target;
// }

// ErrorOr<void> Node::insert_child_recursive(NameString path, AK::RefPtr<Node> const& node)
//{
//     if (path.count() == 0)
//     {
//         return Error::from_string_literal("Path must contain the node name at least!");
//     }
//
//     auto *target = this;
//     if (path.count() > 1 || path.type() == NameString::Type::Absolute || path.depth() != 0)
//     {
//         target = TRY(get_node_at_dirname(m_child, path));
//     }
//
//     auto name = TRY(path.segment(path.count() - 1));
//     return target->insert_child(name, node);
// }

// FIXME: Split off into separate <x>Node files.
static AK::StringView node_data_type_to_string_view(NodeData::Type type)
{
    switch (type) {
    case NodeData::Type::None:
        return "None"sv;
    case NodeData::Type::Byte:
    case NodeData::Type::Word:
    case NodeData::Type::DWord:
    case NodeData::Type::QWord:
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

void NameNode::write_description(AK::StringBuilder& builder)
{
    builder.append(node_data_type_to_string_view(m_data.type()));
    switch (m_data.type()) {
    case NodeData::Type::Byte:
    case NodeData::Type::Word:
    case NodeData::Type::DWord:
    case NodeData::Type::QWord: {
        auto value = m_data.as_integer().release_value_but_fixme_should_propagate_errors();
        builder.appendff(" with value {}, or 0x{:X}", value, value);
        break;
    }
    default:
        break;
    }
}

void MethodNode::write_description(AK::StringBuilder& builder)
{
    builder.appendff("Method(Args: {}, Start: {}, End: {}, Flags: {})", arguments(), start(), end(), flags());
}

}
