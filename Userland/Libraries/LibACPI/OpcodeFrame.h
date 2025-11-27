/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NodeData.h"
#include <AK/Vector.h>

namespace ACPI {
enum class MetaOpcode : u16 {
    // Literal data types.
    NameString = 0xFF00,
    PackageLength = 0xFF01,
    ByteData = 0xFF02,
    WordData = 0xFF03,
    DWordData = 0xFF04,
    QWordData = 0xFF05,
    String = 0xFF06,
    ByteBuffer = 0xFF07,

    // Complex objects.
    FieldElement = 0xFF40,
    TermArg = 0xFF41,
    DataObject = 0xFF42,
    ObjectReference = 0xFF43,
    SimpleName = 0xFF44,
    NullName = 0xFF45,

    ComputationalData = 0xFF46,
    DataRefObject = 0xFF47,
    SuperName = 0xFF48,
    PackageElement = 0xFF49,
    Target = 0xFF4A,

    // Statements
    MarkAsCount = 0xFF80,
    RepeatNextCount = 0xFF81,
    RepeatNextBytes = 0xFF82,
    End = 0xFFFF,
};

using OpcodeFilterFunction = bool (*)(u16);
class OpcodeFrame : public Vector<NodeData> {
public:
    static ErrorOr<OpcodeFrame> from_opcode(u16 opcode, size_t start);

    bool filter_opcode_for_next_operand(u16 opcode);
    ErrorOr<MetaOpcode> begin_operand();
    ErrorOr<void> finish_operand(size_t current_offset);

    u16 opcode() const { return m_opcode; }
    size_t start() const { return m_start; }

    // FIXME: Some better system than this.
    Optional<size_t> counter() const { return m_counter; }
    size_t operand_index() const { return m_operand_index; }
    size_t package_length_index() const { return m_package_length_index; }

private:
    OpcodeFrame(u16 opcode, Span<MetaOpcode> operand_template, size_t start)
        : m_opcode(opcode)
        , m_operand_template(operand_template)
        , m_start(start)
    {
    }
    MetaOpcode peek_next_operand();

    u16 m_opcode;
    Span<MetaOpcode> m_operand_template;
    size_t m_start;

    size_t m_operand_index { 0 };
    size_t m_counter_value_index { 0 };
    size_t m_package_length_index { 0 };
    Optional<size_t> m_counter;
    size_t m_counter_end;

protected:
    friend class Interpreter;
    size_t m_field_offset { 0 };
};

}
