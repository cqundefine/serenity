/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "Node.h"
#include "OpcodeFrame.h"
#include "TableStream.h"
#include <AK/Error.h>
#include <AK/Vector.h>

namespace ACPI {

enum class IntegerKind : u8 {
    Byte,
    Word,
    DWord,
    QWord
};

struct InterpreterCallbacks {
    Function<ErrorOr<u64>(IntegerKind kind, size_t address)> on_read_system_memory;
    Function<ErrorOr<void>(IntegerKind kind, size_t address, u64 value)> on_write_system_memory;
};

template<typename T, bool D>
class FrameStack {
public:
    size_t size() const
    {
        return m_items.size();
    }

    T pop()
    {
        if constexpr (D) {
            dbgln("[LibACPI] FrameStack: Popping frame.");
        }

        T item = m_items[m_items.size() - 1];
        m_items.remove(m_items.size() - 1);
        return item;
    }

    T& top()
    {
        return m_items[m_items.size() - 1];
    }

    T& at(size_t index)
    {
        return m_items.at(index);
    }

    void push(T item)
    {
        if constexpr (D) {
            dbgln("[LibACPI] FrameStack: Pushing frame.");
        }
        m_items.append(item);
    }

private:
    Vector<T> m_items;
};

class ScopeFrame {
public:
    ScopeFrame(RefPtr<Node> root, RefPtr<Node> scope, size_t start, size_t end, size_t table_index)
        : m_root(move(root))
        , m_scope(move(scope))
        , m_start(start)
        , m_end(end)
        , m_table_index(table_index)
    {
    }

    ErrorOr<RefPtr<Node>> find_node(NameString const& path);
    ErrorOr<void> insert_node(NameString const& path, RefPtr<Node> const& node);

    RefPtr<Node> root() { return m_root; }
    RefPtr<Node> scope() { return m_scope; }
    size_t start() const { return m_start; }
    size_t end() const { return m_end; }
    size_t size() const { return m_end - m_start; }

protected:
    RefPtr<Node> m_root, m_scope;
    size_t m_start, m_end, m_table_index;
};

class Interpreter {
public:
    Interpreter(InterpreterCallbacks& callbacks)
        : m_callbacks(callbacks)
    {
    }

    ErrorOr<void> load(TableStream const& stream, bool ignore_block_header);
    ErrorOr<NodeData> evaluate(NameString const& path);
    void print_namespace() const;

protected:
    enum class State : u8 {
        ReadingOpcode,
        ReadingOperand,
        FinalizingOpcode
    };

    ErrorOr<NodeData> evaluate(RefPtr<Node> node);
    ErrorOr<void> evaluate(RefPtr<Node> node, NodeData value_to_write);

    ErrorOr<void> initialize_namespace();
    ErrorOr<IterationDecision> cycle(TableStream& stream);
    ErrorOr<IterationDecision> read_operand(TableStream& stream, MetaOpcode operand);

    ErrorOr<NodeData> finalize_value_opcode(OpcodeFrame& frame);
    ErrorOr<void> finalize_term_opcode(OpcodeFrame& frame, TableStream& stream);

    void debug_print_header(TableStream& stream, size_t operand_index = 0);
    void print_node(RefPtr<Node> const& node, int depth) const;

    State m_state { State::ReadingOpcode };

    AK::Vector<TableStream> m_loaded_tables;
    FrameStack<ScopeFrame, false> m_scope_frames;
    FrameStack<OpcodeFrame, false> m_opcode_frames;

    RefPtr<Node> m_namespace_root;
    InterpreterCallbacks& m_callbacks;
};

}
