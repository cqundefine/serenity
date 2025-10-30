/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include "BlockHeader.h"
#include "Node.h"

namespace ACPI {

class Table : public RefCounted<Table> {
public:
    Table();

    BlockHeader block_header() const { return m_block_header; }
    RefPtr<Node> const& namespace_root() const { return m_namespace_root; }

    void print_namespace() const;
    void print_node(RefPtr<Node> const& node, int depth) const;

protected:
    friend class Interpreter;
    void set_block_header(BlockHeader header) { m_block_header = header; }

    RefPtr<Node> m_namespace_root;
    BlockHeader m_block_header;
};

}
