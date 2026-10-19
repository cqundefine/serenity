/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 * Copyright (c) 2026, Undefine <undefine@undefine.pl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Interpreter.h"
#include "Definitions.h"
#include <AK/Assertions.h>
#include <AK/Debug.h>
#include <AK/RefPtr.h>
#include <AK/StdLibExtras.h>
#include <AK/Stream.h>
#include <LibACPI/NameString.h>
#include <LibACPI/Node.h>
#include <LibACPI/NodeData.h>
#include <LibACPI/OpcodeFrame.h>

#ifdef KERNEL
#    define INTERPRETER_ERROR(fnname, simple, message, ...) \
        do {                                                \
            dbg("[LibACPI] Interpreter::" fnname ": ");     \
            dbgln(message __VA_OPT__(, ) __VA_ARGS__);      \
            return Error::from_errno(EINVAL);               \
        } while (0);
#else
#    define INTERPRETER_ERROR(fnname, simple, message, ...) \
        do {                                                \
            warn("[LibACPI] Interpreter::" fnname ": ");    \
            warnln(message __VA_OPT__(, ) __VA_ARGS__);     \
            return Error::from_string_literal(simple);      \
        } while (0);
#endif

namespace ACPI {

ErrorOr<RefPtr<Node>> ScopeFrame::find_node(NameString const& path)
{
    return Node::find_node(path, scope());
}

ErrorOr<void> ScopeFrame::insert_node(NameString const& path, RefPtr<Node> const& node)
{
    auto target = path.type() == NameString::Type::Relative ? scope() : root();
    dbgln_if(AML_DEBUG, "[LibACPI] Inserting node with name {} at path {}", node->name().to_string_view(), path.to_string());

    for (size_t i = 0; i < path.count() - 1; i++) {
        auto name = TRY(path.segment(i));
        target = TRY(target->find_child(name));
    }

    // Fine to throw error if path has no segments, as the resulting node needs a name.
    auto basename = TRY(path.basename());
    TRY(target->insert_child(basename, node));
    return {};
}

ErrorOr<void> Interpreter::print_namespace() const
{
    TRY(print_node(m_namespace_root, 0));
    return {};
}

ErrorOr<void> Interpreter::print_node(RefPtr<Node> const& node, int depth) const
{
    if (node.is_null())
        return {};

    StringBuilder sb;
    for (auto i = 0; i < depth; i++)
        sb.appendff("| ");

    auto name = node->name().to_string_view();
    sb.appendff("{}{}{}{}: ", name[0], name[1], name[2], name[3]);
    TRY(node->write_description(sb));
    dbgln("{}", sb.string_view());

    auto child = node->child();
    while (!child.is_null()) {
        TRY(print_node(child, depth + 1));
        child = child->neighbour();
    }

    return {};
}

ErrorOr<void> Interpreter::initialize_namespace()
{
    constexpr ssize_t osi_method_table_index = -1;
    constexpr size_t osi_method_start = 0;
    constexpr size_t osi_method_end = 0;
    constexpr size_t osi_method_flags = 1;
    constexpr size_t revision = 2;
    constexpr StringView os_name = "Microsoft Windows NT"sv;
    if (m_namespace_root) {
        return {};
    }

    m_namespace_root = make_ref_counted<DeviceNode>();
    TRY(m_namespace_root->insert_child("_GPE"sv, make_ref_counted<ScopeNode>()));
    TRY(m_namespace_root->insert_child("_PR_"sv, make_ref_counted<ScopeNode>()));
    TRY(m_namespace_root->insert_child("_SI_"sv, make_ref_counted<ScopeNode>()));
    TRY(m_namespace_root->insert_child("_SB_"sv, make_ref_counted<DeviceNode>()));
    TRY(m_namespace_root->insert_child("_TZ_"sv, make_ref_counted<DeviceNode>()));

    TRY(m_namespace_root->insert_child("_REV"sv, make_ref_counted<NameNode>(revision)));
    TRY(m_namespace_root->insert_child("_GL_"sv, make_ref_counted<MutexNode>(0)));
    TRY(m_namespace_root->insert_child("_OSI"sv, make_ref_counted<MethodNode>(osi_method_table_index, osi_method_start, osi_method_end, osi_method_flags)));

    ByteBuffer os_buffer;
    TRY(os_buffer.try_append(os_name.bytes()));
    TRY(m_namespace_root->insert_child("_OS_"sv, make_ref_counted<NameNode>(os_buffer)));
    return {};
}

void Interpreter::debug_print_header(TableStream& stream, size_t operand_index)
{
    if constexpr (AML_DEBUG) {
        dbg("[LibACPI] {:08X} ", stream.offset());
        for (size_t i = 0; i < (m_opcode_frames.size() - 1) - (m_state == State::FinalizingOpcode ? 1 : 0); i++) {
            dbg("|   ");
        }

        switch (m_state) {
        case State::ReadingOpcode:
            dbg("ini");
            break;
        case State::ReadingOperand:
            dbg("opr {}", operand_index);
            break;
        case State::FinalizingOpcode:
            dbg("fin");
            break;
        }
        dbg("        ");
    }
}

ErrorOr<NodeData> Interpreter::finalize_value_opcode(OpcodeFrame& frame)
{
    switch (frame.opcode()) {
    case (u16)Opcode::ZeroOp:
        return 0;
    case (u16)Opcode::OneOp:
        return 1;
    case (u16)Opcode::OnesOp:
        return 0xffffffffffffffff;

    case (u16)Opcode::BytePrefix:
    case (u16)Opcode::WordPrefix:
    case (u16)Opcode::DWordPrefix:
    case (u16)Opcode::QWordPrefix:
        VERIFY(frame[0].type() == NodeData::Type::Integer);
        return frame[0];
    case (u16)Opcode::StringPrefix:
        VERIFY(frame[0].type() == NodeData::Type::String);
        return frame[0];
    case (u16)Opcode::BufferOp:
        VERIFY(frame[2].type() == NodeData::Type::Buffer);
        return frame[2];
    case (u16)Opcode::PackageOp:
        frame.remove(0, 2);
        return NodeData(move(frame));

    case (u16)Opcode::StoreOp: {
        auto value = frame[0];
        auto target = frame[1];

        *TRY(target.as_reference()) = value;
        return value;
    }
    // FIXME: Implement all other arithmetic opcodes.
    case (u16)Opcode::AddOp:
    case (u16)Opcode::ShiftLeftOp: {
        auto lhs = TRY(frame[0].as_integer());
        auto rhs = TRY(frame[1].as_integer());
        auto target = frame[2];

        // FIXME: Add support for storing data into nodes.
        VERIFY(TRY(target.as_integer()) == 0);

        i64 result;
        // FIXME: Hopefully get rid of the inner switch
        switch (frame.opcode()) {
        case (u16)Opcode::AddOp:
            result = lhs + rhs;
            break;
        case (u16)Opcode::ShiftLeftOp:
            result = lhs << rhs;
            break;
        default:
            VERIFY_NOT_REACHED();
        }
        return result;
    }
    case (u16)Opcode::LNotOp: {
        auto value = TRY(frame[0].as_integer());
        return value == 0;
    }
    // FIXME: Implement all other logical opcodes.
    case (u16)Opcode::LAndOp:
    case (u16)Opcode::LOrOp:
    case (u16)Opcode::LEqualOp:
    case (u16)Opcode::LGreaterOp:
    case (u16)Opcode::LLessOp: {
        auto lhs = TRY(frame[0].as_integer());
        auto rhs = TRY(frame[1].as_integer());

        i64 result;
        // FIXME: Hopefully get rid of the inner switch
        switch (frame.opcode()) {
        case (u16)Opcode::LAndOp:
            result = lhs && rhs;
            break;
        case (u16)Opcode::LOrOp:
            result = lhs || rhs;
            break;
        case (u16)Opcode::LEqualOp:
            result = lhs == rhs;
            break;
        case (u16)Opcode::LGreaterOp:
            result = lhs > rhs;
            break;
        case (u16)Opcode::LLessOp:
            result = lhs < rhs;
            break;
        default:
            VERIFY_NOT_REACHED();
        }
        return result;
    }
    case (u16)Opcode::CondRefOfOp:
    case (u16)MetaOpcode::MethodCall:
        // FIXME: Implement this
        return 0;
    default:
        INTERPRETER_ERROR("finalize_value_opcode", "Unimplemented!", "Unimplemented value opcode {:04X}!", frame.opcode());
    }
    VERIFY_NOT_REACHED();
}

ErrorOr<void> Interpreter::finalize_term_opcode(OpcodeFrame& frame, TableStream& stream)
{
    switch (frame.opcode()) {
    case (u16)Opcode::ScopeOp: {
        auto length = TRY(frame[0].as_integer());
        auto path = TRY(frame[1].as_namestring());
        auto scope = TRY(m_scope_frames.top().find_node(path));

        auto scope_frame = ScopeFrame(m_namespace_root, scope, frame.start(), frame.start() + length + 1, m_loaded_tables.size() - 1);
        TRY(m_scope_frames.push(scope_frame));
        return {};
    }
    case (u16)Opcode::ProcessorOp: {
        auto length = TRY(frame[0].as_integer());
        auto path = TRY(frame[1].as_namestring());
        auto id = TRY(frame[2].as_integer());
        auto block_address = TRY(frame[3].as_integer());
        auto block_length = TRY(frame[4].as_integer());

        auto node = make_ref_counted<ProcessorNode>(block_address, id, block_length);
        TRY(m_scope_frames.top().insert_node(path, node));

        auto scope_frame = ScopeFrame(m_namespace_root, node, frame.start(), frame.start() + length + 1, m_loaded_tables.size() - 1);
        TRY(m_scope_frames.push(scope_frame));
        return {};
    }
    case (u16)Opcode::ThermalZoneOp: {
        auto length = TRY(frame[0].as_integer());
        auto path = TRY(frame[1].as_namestring());

        auto node = make_ref_counted<ThermalZoneNode>();
        TRY(m_scope_frames.top().insert_node(path, node));

        auto scope_frame = ScopeFrame(m_namespace_root, node, frame.start(), frame.start() + length + 1, m_loaded_tables.size() - 1);
        TRY(m_scope_frames.push(scope_frame));
        return {};
    }
    case (u16)Opcode::NameOp: {
        auto path = TRY(frame[0].as_namestring());
        auto data = frame[1];

        auto node = make_ref_counted<NameNode>(data);
        TRY(m_scope_frames.top().insert_node(path, node));
        return {};
    }
    case (u16)Opcode::OpRegionOp: {
        auto path = TRY(frame[0].as_namestring());
        auto region_space = TRY(frame[1].as_integer());
        auto region_offset = TRY(frame[2].as_integer());
        auto region_length = TRY(frame[3].as_integer());

        auto node = make_ref_counted<OperationRegionNode>(region_space, region_offset, region_length);
        TRY(m_scope_frames.top().insert_node(path, node));
        return {};
    }
    case (u16)Opcode::AliasOp: {
        auto path = TRY(frame[0].as_namestring());
        auto alias = TRY(frame[1].as_namestring());

        auto node = make_ref_counted<AliasNode>(alias);
        TRY(m_scope_frames.top().insert_node(path, node));
        return {};
    }
    case (u16)Opcode::PowerResOp: {
        auto length = TRY(frame[0].as_integer());
        auto path = TRY(frame[1].as_namestring());

        auto node = make_ref_counted<PowerResourceNode>();
        TRY(m_scope_frames.top().insert_node(path, node));

        auto scope_frame = ScopeFrame(m_namespace_root, node, frame.start(), frame.start() + length + 1, m_loaded_tables.size() - 1);
        TRY(m_scope_frames.push(scope_frame));
        return {};
    }
    case (u16)Opcode::IfOp: {
        auto length = TRY(frame[0].as_integer()) - (stream.offset() - frame.start()) + 1;
        auto condition = TRY(frame[1].as_integer());

        m_last_if_condition = condition != 0;

        if (!m_last_if_condition) {
            // Skip over termlist.
            TRY(stream.seek(length, SeekMode::FromCurrentPosition));
        }
        return {};
    }
    case (u16)Opcode::ElseOp: {
        auto length = TRY(frame[0].as_integer()) - (stream.offset() - frame.start()) + 1;

        if (m_last_if_condition) {
            // Skip over termlist.
            TRY(stream.seek(length, SeekMode::FromCurrentPosition));
        }
        return {};
    }
    case (u16)Opcode::MutexOp: {
        auto path = TRY(frame[0].as_namestring());
        auto sync_flags = TRY(frame[1].as_integer());

        auto node = make_ref_counted<MutexNode>(sync_flags);
        TRY(m_scope_frames.top().insert_node(path, node));
        return {};
    }
    case (u16)Opcode::DeviceOp: {
        auto length = TRY(frame[0].as_integer());
        auto path = TRY(frame[1].as_namestring());

        auto node = make_ref_counted<DeviceNode>();
        TRY(m_scope_frames.top().insert_node(path, node));

        auto scope_frame = ScopeFrame(m_namespace_root, node, frame.start(), frame.start() + length + 1, m_loaded_tables.size() - 1);
        TRY(m_scope_frames.push(scope_frame));
        return {};
    }
    case (u16)Opcode::MethodOp: {
        auto length = TRY(frame[0].as_integer()) - (stream.offset() - frame.start()) + 1;
        auto path = TRY(frame[1].as_namestring());
        auto flags = TRY(frame[2].as_integer());

        auto node = make_ref_counted<MethodNode>(m_loaded_tables.size() - 1, stream.offset(), length, flags);
        TRY(m_scope_frames.top().insert_node(path, node));

        // Skip over termlist.
        TRY(stream.seek(length, SeekMode::FromCurrentPosition));
        return {};
    }
    case (u16)Opcode::FieldOp:
        // DefField := FieldOp PkgLength NameString FieldFlags FieldList
        m_opcode_frames.top().m_field_offset = 0;
        return {};
    case (u16)Opcode::IndexFieldOp: {
        // FIXME: Actually parse this
        // auto length = TRY(frame[0].as_integer()) - (stream.offset() - frame.start()) + 2;
        // TRY(stream.seek(length, SeekMode::FromCurrentPosition));
        m_opcode_frames.top().m_field_offset = 0;
        return {};
    }
    case (u16)Opcode::CreateByteFieldOp:
    case (u16)Opcode::CreateWordFieldOp:
    case (u16)Opcode::CreateDWordFieldOp:
    case (u16)Opcode::CreateQWordFieldOp:
    case (u16)Opcode::CreateBitFieldOp: {
        // FIXME: Remove inner switch statement.
        size_t size = 0;
        switch (frame.opcode()) {
        case (u16)Opcode::CreateByteFieldOp:
            size = 8;
            break;
        case (u16)Opcode::CreateWordFieldOp:
            size = 16;
            break;
        case (u16)Opcode::CreateDWordFieldOp:
            size = 32;
            break;
        case (u16)Opcode::CreateQWordFieldOp:
            size = 64;
            break;
        case (u16)Opcode::CreateBitFieldOp:
            size = 1;
            break;
        default:
            VERIFY_NOT_REACHED();
        }

        auto* buffer = TRY(frame[0].as_buffer_ptr());
        auto index = TRY(frame[1].as_integer());
        auto path = TRY(frame[2].as_namestring());

        auto offset = index * (size == 1 ? 1 : 8);
        auto node = make_ref_counted<BufferFieldNode>(buffer, offset, size);
        TRY(m_scope_frames.top().insert_node(path, node));
        return {};
    }
    case (u16)Opcode::CreateFieldOp: {
        auto* buffer = TRY(frame[0].as_buffer_ptr());
        auto index = TRY(frame[1].as_integer());
        auto size = TRY(frame[2].as_integer());
        auto path = TRY(frame[3].as_namestring());

        auto offset = index * (size == 1 ? 1 : 8);
        auto node = make_ref_counted<BufferFieldNode>(buffer, offset, size);
        TRY(m_scope_frames.top().insert_node(path, node));
        return {};
    }
    case (u16)Opcode::StoreOp: {
        auto maybe_error = finalize_value_opcode(frame);
        if (maybe_error.is_error())
            return maybe_error.release_error();
        return {};
    }
    default:
        break;
    }
    INTERPRETER_ERROR("finalize_term_opcode", "Unimplemented!", "Unimplemented term opcode {:04X}!", frame.opcode());
}

ErrorOr<IterationDecision> Interpreter::read_operand(TableStream& stream, MetaOpcode operand)
{
    auto& top_opcode_frame = m_opcode_frames.top();
    auto& top_scope_frame = m_scope_frames.top();
    switch (operand) {
    case MetaOpcode::FieldElement: {
        dbg_if(AML_DEBUG, "Reading field element...");
        if (TableStream::is_lead_name_char(TRY(stream.peek_value<u8>()))) {
            // NamedField := NameSeg PkgLength
            // As is ACPI tradition, no opcode, just bare name segment.
            auto name_segment_bytes = TRY(stream.read_in_place<u8>(4));
            auto name_segment = TRY(NameSegment::from_readonly_bytes(name_segment_bytes));
            auto package_length = TRY(stream.read_package_length());

            auto operation_region_node = TRY(top_scope_frame.find_node(TRY(top_opcode_frame[1].as_namestring())));
            auto flags = TRY(top_opcode_frame[2].as_integer());

            dbgln_if(AML_DEBUG, " ({}, {})", name_segment.to_string_view(), package_length);

            Field field(operation_region_node, flags, top_opcode_frame.m_field_offset, package_length);
            auto node = make_ref_counted<FieldNode>(field);

            TRY(m_scope_frames.top().scope()->insert_child(name_segment, node));
            TRY(top_opcode_frame.finish_operand(stream.offset()));
            top_opcode_frame.m_field_offset += package_length;
            return IterationDecision::Continue;
        }

        auto type = TRY(stream.read_value<u8>());
        dbgln_if(AML_DEBUG, " Type: {}", type);
        switch (type) {
        case 0x00: {
            // ReservedField := 0x00 PkgLength
            top_opcode_frame.m_field_offset += TRY(stream.read_package_length());
            TRY(top_opcode_frame.finish_operand(stream.offset()));
            return IterationDecision::Continue;
        }
        default: {
            INTERPRETER_ERROR("load", "Unimplemented field type!", "Unimplemented field type {:02X}!", type);
        }
        }

        break;
    }
    case MetaOpcode::IndexFieldElement: {
        dbg_if(AML_DEBUG, "Reading index field element...");
        if (TableStream::is_lead_name_char(TRY(stream.peek_value<u8>()))) {
            // NamedField := NameSeg PkgLength
            // As is ACPI tradition, no opcode, just bare name segment.
            auto name_segment_bytes = TRY(stream.read_in_place<u8>(4));
            auto name_segment = TRY(NameSegment::from_readonly_bytes(name_segment_bytes));
            auto package_length = TRY(stream.read_package_length());

            dbgln_if(AML_DEBUG, " ({}, {})", name_segment.to_string_view(), package_length);

            auto node = make_ref_counted<IndexFieldNode>();

            TRY(m_scope_frames.top().scope()->insert_child(name_segment, node));
            TRY(top_opcode_frame.finish_operand(stream.offset()));
            top_opcode_frame.m_field_offset += package_length;
            return IterationDecision::Continue;
        }

        auto type = TRY(stream.read_value<u8>());
        dbgln_if(AML_DEBUG, " Type: {}", type);
        switch (type) {
        case 0x00: {
            // ReservedField := 0x00 PkgLength
            top_opcode_frame.m_field_offset += TRY(stream.read_package_length());
            TRY(top_opcode_frame.finish_operand(stream.offset()));
            return IterationDecision::Continue;
        }
        default: {
            INTERPRETER_ERROR("load", "Unimplemented field type!", "Unimplemented field type {:02X}!", type);
        }
        }

        break;
    }
    case MetaOpcode::NameString: {
        auto value = TRY(stream.read_name_string());
        dbgln_if(AML_DEBUG, "Reading namestring... ({})", value.to_string());
        TRY(top_opcode_frame.try_append(value));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::PackageLength: {
        auto length = TRY(stream.read_package_length());
        dbgln_if(AML_DEBUG, "Reading package length... 0x({:X})", length);
        TRY(top_opcode_frame.try_append(NodeData((u64)length)));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::ByteData: {
        auto value = TRY(stream.read_value<u8>());
        dbgln_if(AML_DEBUG, "Reading byte... ({})", value);
        TRY(top_opcode_frame.try_append(NodeData((u8)value)));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::WordData: {
        auto value = TRY(stream.read_value<u16>());
        dbgln_if(AML_DEBUG, "Reading word... ({})", value);
        TRY(top_opcode_frame.try_append(NodeData((u16)value)));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::DWordData: {
        auto value = TRY(stream.read_value<u32>());
        dbgln_if(AML_DEBUG, "Reading dword... ({})", value);
        TRY(top_opcode_frame.try_append(NodeData((u32)value)));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::QWordData: {
        auto value = TRY(stream.read_value<u64>());
        dbgln_if(AML_DEBUG, "Reading qword... ({})", value);
        TRY(top_opcode_frame.try_append(NodeData((u64)value)));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::String: {
        dbgln_if(AML_DEBUG, "Reading string...");
        auto value = TRY(stream.read_string());
        TRY(top_opcode_frame.try_append(NodeData(value)));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }
    case MetaOpcode::ByteBuffer: {
        auto package_length = TRY(top_opcode_frame[top_opcode_frame.package_length_index()].as_integer());
        auto start = top_opcode_frame.start();
        auto buffer_data_length = package_length - (stream.offset() - start - 1);

        auto count = top_opcode_frame.counter().value();
        dbgln_if(AML_DEBUG, "Reading byte buffer of size {}...", (ssize_t)count);
        ByteBuffer buffer;
        buffer.resize(count);

        for (size_t i = 0; i < buffer_data_length; i++)
            buffer[i] = TRY(stream.read_value<u8>());
        TRY(top_opcode_frame.try_append(NodeData(Buffer(buffer))));
        TRY(top_opcode_frame.finish_operand(stream.offset()));
        break;
    }

    case MetaOpcode::PackageElement: {
        dbg_if(AML_DEBUG, "Reading package element {:04X}...", (u16)operand);

        auto initial = TRY(stream.peek_value<u8>());
        if (TableStream::is_namestring_start(initial)) {
            auto ns = TRY(stream.read_name_string());
            dbgln_if(AML_DEBUG, " ({})", ns.to_string());
            TRY(top_opcode_frame.try_append(ns));
            TRY(top_opcode_frame.finish_operand(stream.offset()));
            return IterationDecision::Continue;
        }

        dbgln_if(AML_DEBUG, "");
        m_state = State::ReadingOpcode;
        return IterationDecision::Continue;
    }

    case MetaOpcode::Target:
    case MetaOpcode::SuperName:
        top_opcode_frame.m_expects_reference = true;
        [[fallthrough]];
    case MetaOpcode::DataRefObject:
    case MetaOpcode::TermArg: {
        dbgln_if(AML_DEBUG, "Reading complex object {:04X}...", (u16)operand);

        m_state = State::ReadingOpcode;
        return IterationDecision::Continue;
    }
    default: {
        dbgln("Failure!");
        INTERPRETER_ERROR("load", "Unimplemented operand type!", "Operand type {:04X} unimplemented!", (u16)operand);
    }
    }
    return IterationDecision::Continue;
}

static size_t round_to_next_power_of_2(size_t n)
{
    // FIXME: Figure out how AK::round_up_to_power_of_two works and replace this.
    size_t v = 1;
    while (v < n) {
        v *= 2;
    }
    return v;
}

ErrorOr<NodeData> Interpreter::evaluate(NameString const& path)
{
    auto node = TRY(Node::find_node(path, m_namespace_root));
    return evaluate(node);
}

ErrorOr<NodeData> Interpreter::evaluate(RefPtr<Node> node)
{
    if (node->type() == NodeType::Field) {
        auto field_node = static_ptr_cast<FieldNode>(node);
        auto field = field_node->field();
        auto opregion = static_ptr_cast<OperationRegionNode>(field.operation_region());

        IntegerKind kind = IntegerKind::QWord;
        switch (field.access()) {
        case FieldAccess::Any: {
            size_t size = round_to_next_power_of_2(field.size());

            u64 max_bitsize = 32;
            if (opregion->space() == RegionSpace::SystemMemory)
                max_bitsize = 64;

            size = AK::clamp(size, 8, max_bitsize);
            switch (size) {
            case 8:
                kind = IntegerKind::Byte;
                break;
            case 16:
                kind = IntegerKind::Word;
                break;
            case 32:
                kind = IntegerKind::DWord;
                break;
            case 64:
                kind = IntegerKind::QWord;
                break;
            default:
                INTERPRETER_ERROR("evaluate", "Invalid access size!", "Invalid access size: {}", size);
            }
            break;
        }
        case FieldAccess::Byte:
            kind = IntegerKind::Byte;
            break;
        case FieldAccess::Word:
            kind = IntegerKind::Word;
            break;
        case FieldAccess::DWord:
            kind = IntegerKind::DWord;
            break;
        case FieldAccess::QWord:
            kind = IntegerKind::QWord;
            break;
        default:
            INTERPRETER_ERROR("evaluate", "Invalid field access!", "Invalid access: {}", (u32)field.access());
        }
        (void)kind;

        // FIXME: Implement missing region spaces.
        u64 address = opregion->offset() + field.offset();
        switch (opregion->space()) {
        case RegionSpace::SystemMemory:
            return 0;
            // return TRY(m_callbacks.on_read_system_memory(kind, address));
        case RegionSpace::PciConfig:
            dbgln("[LibACPI] PciConfig read from address {:08X}, but PciConfig reads not supported yet. Returning 0.", address);
            return 0;
        default:
            INTERPRETER_ERROR("evaluate", "Operating region space not supported!", "Operating region space {:02X} not supported for evaluation!", opregion->space_raw());
        }
        VERIFY_NOT_REACHED();
    } else if (node->type() == NodeType::Name) {
        auto name_node = static_ptr_cast<NameNode>(node);
        return &name_node->data();
    } else if (node->type() == NodeType::IndexField) {
        dbgln("[LibACPI] Interpreter::cycle: Index fields unsupported!");
        return 0;
    } else if (node->type() == NodeType::Method) {
        dbgln("[LibACPI] Interpreter::cycle: Methods unsupported!");
        // FIXME: This is very much not correct but should be fine for now
        return 0;
    } else {
        return NodeData();
    }

    INTERPRETER_ERROR("evaluate", "Cannot evaluate node type!", "Node type {} not supported for evaluation!", node->kind());
}

ErrorOr<IterationDecision> Interpreter::cycle(TableStream& stream)
{
    // Stacks have already been initialized.
    auto& top_opcode_frame = m_opcode_frames.top();

    switch (m_state) {
    case Interpreter::State::ReadingOpcode: {
        // Pop all scope frames that we've exited.
        // Do it here in case any pending opcodes need to be finalized at EOF.
        while (m_scope_frames.size() != 0) {
            auto& scope_frame = m_scope_frames.top();
            if (stream.offset() < scope_frame.end()) {
                break;
            }

            m_scope_frames.pop();
        }

        if (stream.is_eof())
            return IterationDecision::Break;

        debug_print_header(stream);

        auto start = stream.offset();
        auto opcode = TRY(stream.peek_opcode());
        if (TableStream::is_namestring_start(opcode)) {
            auto name = TRY(stream.read_name_string());
            dbgln_if(AML_DEBUG, "{}", name.to_string());

            auto maybe_node = m_scope_frames.top().find_node(name);

            NodeData value {};
            if (!maybe_node.is_error()) {
                auto node = maybe_node.release_value();
                if (top_opcode_frame.m_expects_reference) {
                    TRY(top_opcode_frame.try_append(NodeData(node)));
                    TRY(top_opcode_frame.finish_operand(stream.offset()));
                    return IterationDecision::Continue;
                }

                if (node->type() == NodeType::Method) {
                    auto method_node = static_ptr_cast<MethodNode>(node);

                    Vector<MetaOpcode> operand_template;
                    TRY(operand_template.try_ensure_capacity(method_node->arguments() + 1));
                    for (u8 i = 0; i < method_node->arguments(); i++) {
                        operand_template.unchecked_append(MetaOpcode::TermArg);
                    }
                    operand_template.unchecked_append(MetaOpcode::End);

                    auto opcode_frame = OpcodeFrame::from_opcode_with_operand_template((u16)MetaOpcode::MethodCall, move(operand_template), start);
                    TRY(m_opcode_frames.push(move(opcode_frame)));
                    m_state = State::ReadingOperand;
                    return IterationDecision::Continue;
                } else {
                    value = TRY(evaluate(node));
                }
            }

            TRY(top_opcode_frame.try_append(value));
            TRY(top_opcode_frame.finish_operand(stream.offset()));

            m_state = State::ReadingOperand;
            return IterationDecision::Continue;
        }

        if (!top_opcode_frame.filter_opcode_for_next_operand(opcode)) {
            dbgln("Failure!");
            INTERPRETER_ERROR("load", "Filtered opcode!", "Argument opcode {:04X} filtered by opcode {:04X}!", opcode, top_opcode_frame.opcode());
        }

        if ((opcode < 256) && TableStream::is_lead_name_char(opcode)) {
            auto name_span = TRY(stream.read_in_place<u8>(4));
            auto name = StringView(name_span);

            INTERPRETER_ERROR("load", "Unimplemented method invocation!", "Tried to invoke method {}!", name);
        } else {
            (void)TRY(stream.read_opcode());
        }

        dbgln_if(AML_DEBUG, "Opcode: {:04X}", opcode);

        auto opcode_frame = TRY(OpcodeFrame::from_opcode(opcode, start));
        TRY(m_opcode_frames.push(move(opcode_frame)));
        m_state = State::ReadingOperand;
        return IterationDecision::Continue;
    }

    case Interpreter::State::ReadingOperand: {
        auto index = top_opcode_frame.operand_index();

        auto operand = TRY(top_opcode_frame.begin_operand());
        if (operand == MetaOpcode::End) {
            m_state = State::FinalizingOpcode;
            return IterationDecision::Continue;
        }

        debug_print_header(stream, index);
        return read_operand(stream, operand);
    }

    case Interpreter::State::FinalizingOpcode: {
        debug_print_header(stream);

        auto top_frame_value = m_opcode_frames.pop();
        if (m_opcode_frames.size() > 1) {
            dbgln_if(AML_DEBUG, "Finalizing argument {:04X}", top_frame_value.opcode());
            auto value = TRY(finalize_value_opcode(top_frame_value));
            TRY(m_opcode_frames.top().try_append(value));
            TRY(m_opcode_frames.top().finish_operand(stream.offset()));
            m_state = State::ReadingOperand;
        } else {
            dbgln_if(AML_DEBUG, "Finalizing term {:04X}", top_frame_value.opcode());
            TRY(finalize_term_opcode(top_frame_value, stream));
            m_state = State::ReadingOpcode;
        }

        return IterationDecision::Continue;
    }
    }

    // TODO: Is this unreachable
    return IterationDecision::Continue;
}

ErrorOr<void> Interpreter::load(TableStream const& stream, bool ignore_block_header)
{
    // AMLCode := DefBlockHeader TermList
    TRY(m_loaded_tables.try_append(stream));

    auto& stream_ref = m_loaded_tables.last();

    // Bail on incorrect checksum.
    if (!ignore_block_header) {
        auto checksum = TRY(stream_ref.generate_checksum());
        if (checksum != 0) {
            INTERPRETER_ERROR("load", "Incorrect checksum!", "Checksum incorrect, expected 0 but got {}!", checksum);
        }

        // Read table's block header.
        // FIXME: Figure out what to do with the data apart from logging.
        auto block_header = TRY(stream_ref.read_table_block_header());
        dbgln("[LibACPI] Interpreter::load[{}]: Loading table {} by oem {}", m_loaded_tables.size() - 1, StringView(block_header.oem_table_id().span()), StringView(block_header.oem_id().span()));
    }
    // Initialize the namespace, alongside initial nodes.
    TRY(initialize_namespace());

    // Initial frame stacks.
    auto root_scope_frame = ScopeFrame(m_namespace_root, m_namespace_root, stream_ref.offset(), TRY(stream_ref.size()), m_loaded_tables.size() - 1);
    TRY(m_scope_frames.push(move(root_scope_frame)));

    auto root_opcode_frame = TRY(OpcodeFrame::from_opcode((u16)MetaOpcode::End, stream_ref.offset()));
    TRY(m_opcode_frames.push(move(root_opcode_frame)));

    while (m_opcode_frames.size() > 0 && m_scope_frames.size() > 0) {
        auto result = TRY(cycle(stream_ref));
        if (result == IterationDecision::Break) {
            break;
        }
    }

    dbgln("Scope frames: {}", m_scope_frames.size());
    dbgln("Opcode frames: {}", m_opcode_frames.size());
    if (m_scope_frames.size() != 0 || m_opcode_frames.size() != 1) {
        return Error::from_string_view_or_print_error_and_return_errno("[LibACPI] Interpreter::load: Unbalanced scope or opcode frames!"sv, EINVAL);
    }

    (void)m_opcode_frames.pop();
    return {};
}

}
