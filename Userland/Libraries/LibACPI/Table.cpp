/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Table.h"

namespace ACPI {

Table::Table()
{
    m_namespace_root = make_ref_counted<DeviceNode>();
    m_namespace_root->insert_child("_SB_"sv, make_ref_counted<DeviceNode>()).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_TZ_"sv, make_ref_counted<DeviceNode>()).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_PR_"sv, make_ref_counted<ScopeNode>()).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_SI_"sv, make_ref_counted<ScopeNode>()).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_GPE"sv, make_ref_counted<ScopeNode>()).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_DS_"sv, make_ref_counted<DeviceNode>()).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_REV"sv, make_ref_counted<NameNode>(NodeData(1))).release_value_but_fixme_should_propagate_errors();
    m_namespace_root->insert_child("_OSI"sv, make_ref_counted<NameNode>(NodeData(0))).release_value_but_fixme_should_propagate_errors();
}

void Table::print_namespace() const
{
    auto current = m_namespace_root;
    print_node(current, 0);
}

void Table::print_node(RefPtr<Node> const& node, int depth) const
{
    if (node.is_null())
        return;

    StringBuilder sb;
    sb.append_repeated(' ', depth + depth);

    auto name = node->name().to_string_view();
    sb.appendff("{}{}{}{}: ", name[0], name[1], name[2], name[3]);
    node->write_description(sb);
    dbgln("{}", sb.string_view());

    auto child = node->child();
    while (!child.is_null()) {
        print_node(child, depth + 1);
        child = child->neighbour();
    }
}

}
