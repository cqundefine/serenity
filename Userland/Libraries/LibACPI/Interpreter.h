/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include "Table.h"
#include "TableReader.h"
#include <AK/Error.h>
#include <AK/OwnPtr.h>
#include <AK/String.h>
#include <AK/Variant.h>
#include <AK/Vector.h>

namespace ACPI {

class ParseFrame {
public:
    ParseFrame(RefPtr<Node> const& scope, size_t end)
        : m_scope(scope)
        , m_end(end)
    {
    }

    RefPtr<ACPI::Node> node() const { return m_scope; }
    size_t end() const { return m_end; }

protected:
    RefPtr<ACPI::Node> m_scope;
    size_t m_end;
};

class Interpreter {
public:
    Interpreter()
        : m_table(nullptr)
    {
    }
    ErrorOr<RefPtr<ACPI::Table>> interpret(u8* buffer, size_t length);

protected:
    Vector<ParseFrame> m_parse_frames;
    RefPtr<ACPI::Table> m_table;

    void push_parse_frame(ParseFrame const& frame);
    ParseFrame pop_parse_frame();

    ErrorOr<void> read_term(TableReader& reader, ParseFrame const& frame);

    ErrorOr<RefPtr<Node>> find_node(NameString const& path, RefPtr<Node> const& scope);
    ErrorOr<void> insert_node(NameString const& path, RefPtr<Node> const& scope, RefPtr<Node> const& node);

    ErrorOr<u32> read_name_segment(TableReader& reader);
    ErrorOr<NodeData> read_computational_data(TableReader& reader, ParseFrame const& frame, u16 opcode);
    ErrorOr<NodeData> read_data_ref_object(TableReader& reader, ParseFrame const& frame);
    ErrorOr<NodeData> read_data_object(TableReader& reader, ParseFrame const& frame, u16 opcode);
    ErrorOr<NodeData> read_def_buffer(TableReader& reader, ParseFrame const& frame);
    ErrorOr<NodeData> read_term_arg(TableReader& reader, ParseFrame const& frame);
    ErrorOr<NodeData> read_package(TableReader& reader, ParseFrame const& frame, u16 opcode);

    ErrorOr<void> process_field_element(TableReader& reader, ParseFrame const& frame, Field const& field);
    ErrorOr<void> process_def_name(TableReader& reader, ParseFrame const& frame);
    ErrorOr<void> process_def_scope(TableReader& reader, ParseFrame const& frame);
    ErrorOr<void> process_def_device(TableReader& reader, ParseFrame const& frame);
    ErrorOr<void> process_def_operation_region(TableReader& reader, ParseFrame const& frame);
    ErrorOr<void> process_def_field(TableReader& reader, ParseFrame const& frame);
    ErrorOr<void> process_def_method(TableReader& reader, ParseFrame const& frame);
    ErrorOr<void> process_def_processor(TableReader& reader, ParseFrame const& frame);
};

}
