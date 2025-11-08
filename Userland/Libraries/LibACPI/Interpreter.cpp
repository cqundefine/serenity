/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Interpreter.h"
#include "BlockHeader.h"
#include "Definitions.h"
#include "NameString.h"
#include "Table.h"
#include <AK/NonnullOwnPtr.h>
#include <AK/NonnullRefPtr.h>

namespace ACPI {

#define UNIMPLEMENTED_OPCODE(fn, op)                                \
    do {                                                            \
        warnln("[LibACPI] Interpreter::" fn ": Opcode {:04X}", op); \
        return Error::from_string_literal("Unimplemented");         \
    } while (0)

ErrorOr<void> Interpreter::insert_node(NameString const& path, RefPtr<Node> const& scope, RefPtr<Node> const& node)
{
    auto target = m_table->namespace_root();

    if (path.type() == NameString::Type::Relative) {
        if (path.count() > 1) {
            // Node::find_node handles path depth.
            auto dirname = TRY(path.dirname());
            target = TRY(Node::find_node(dirname, scope));
        } else {
            target = scope;

            // Can't use find_child_recursive, have to handle the path depth manually.
            for (size_t i = 0; i < path.depth(); i++) {
                target = target->parent();
                if (target.is_null()) {
                    return Error::from_string_literal("Path depth overflows root!");
                }
            }
        }
    }

    // Fine to throw error if path has no segments, as the resulting node needs a name.
    auto basename = TRY(path.basename());
    TRY(target->insert_child(basename, node));
    return {};
}

ErrorOr<RefPtr<Node>> Interpreter::find_node(NameString const& path, RefPtr<Node> const& scope)
{
    return Node::find_node(path, scope);
}

ErrorOr<void> Interpreter::process_def_method(TableReader& reader, ParseFrame const& frame)
{
    // DefMethod := MethodOp PkgLength NameString MethodFlags TermList
    auto start = reader.position() - 1;

    auto package_length = reader.package_length();
    auto path = TRY(NameString::from_reader(reader));
    auto flags = reader.byte();

    auto term_list_start = reader.position();
    auto node = make_ref_counted<MethodNode>(term_list_start, start + package_length, flags);

    TRY(insert_node(path, frame.node(), node));
    reader.set_position(start + package_length + 1);

    return {};
}

ErrorOr<void> Interpreter::process_def_scope(TableReader& reader, ParseFrame const& frame)
{
    // DefScope := ScopeOp PkgLength NameString TermList
    auto start = reader.position() - 1;
    auto package_length = reader.package_length();
    auto name_string = TRY(NameString::from_reader(reader));
    auto scope = TRY(Node::find_node(name_string, frame.node()));

    auto f = ParseFrame(scope, reader.position(), start + package_length);
    push_parse_frame(f);

    return {};
}

ErrorOr<void> Interpreter::process_def_device(TableReader& reader, ParseFrame const& frame)
{
    // DefDevice := DeviceOp PkgLength NameString TermList
    auto start = reader.position() - 1;
    auto package_length = reader.package_length();
    auto path = TRY(NameString::from_reader(reader));

    auto node = make_ref_counted<DeviceNode>();
    TRY(insert_node(path, frame.node(), node));

    auto new_frame = ParseFrame(node, reader.position(), start + package_length);
    push_parse_frame(new_frame);

    return {};
}

ErrorOr<void> Interpreter::process_def_processor(TableReader& reader, ParseFrame const& frame)
{
    // DefProcessor := ProcessorOp PkgLength NameString ProcID PblkAddr PblkLen TermList
    auto start = reader.position() - 1;
    auto package_length = reader.package_length();
    auto path = TRY(NameString::from_reader(reader));
    auto id = reader.byte();
    auto block_address = reader.dword();
    auto block_length = reader.byte();

    auto node = make_ref_counted<ProcessorNode>(block_address, id, block_length);
    TRY(insert_node(path, frame.node(), node));

    // FIXME: Figure out what can actually be inside a Processor node.
    auto new_frame = ParseFrame(node, reader.position(), start + package_length);
    push_parse_frame(new_frame);

    return {};
}

ErrorOr<void> Interpreter::process_field_element(TableReader& reader, ParseFrame const& frame, Field const& field)
{
    (void)frame;
    (void)field;
    // FieldElement := NamedField | ReservedField | AccessField | ExtendedAccessField | ConnectField

    // AccessType   := ByteData
    // AccessAttrib := ByteData
    //
    // ExtendedAccessAttrib := ByteData (0xB, 0xE, 0xF)
    // AccessField         := 0x01 AccessType AccessAttrib
    // ConnectField        := 0x02 NameString | 0x02 BufferData
    // ExtendedAccessField := 0x03 AccessType ExtendedAccessAttrib AccessLength

    if (TableReader::is_lead_name_char(reader.peek())) {
        // NamedField := NameSeg PkgLength
        // As is ACPI tradition, no opcode, just bare name segment.
        auto segment = TRY(reader.name_segment());
        auto package_length = reader.package_length();

        (void)package_length;
        auto node = make_ref_counted<FieldNode>(field);
        TRY(frame.node()->insert_child(segment, node));

        return {};
    }

    u16 opcode = reader.opcode();
    switch (opcode) {
    case 0x00: {
        // ReservedField := 0x00 PkgLength
        // FIXME: Figure out what to do with this data.
        auto package_length = reader.package_length();
        (void)package_length;
        return {};
    }
    default:
        // FIXME: Implement remaining fields.
        break;
    }

    UNIMPLEMENTED_OPCODE("process_field_element", opcode);
}

ErrorOr<void> Interpreter::process_def_field(TableReader& reader, ParseFrame const& frame)
{
    // DefField := FieldOp PkgLength NameString FieldFlags FieldList
    auto start = reader.position() - 1;
    auto package_length = reader.package_length();
    auto path = TRY(NameString::from_reader(reader));
    auto field_flags = reader.byte();

    auto operation_region_node = TRY(Node::find_node(path, frame.node()));

    // FIXME: Maybe handle this in the main interpreter loop to avoid stack growth?
    Field field(operation_region_node, field_flags);
    while (reader.position() < start + package_length) {
        TRY(process_field_element(reader, frame, field));
    }

    (void)package_length;
    (void)field_flags;
    (void)path;
    return {};
}

ErrorOr<void> Interpreter::process_def_name(TableReader& reader, ParseFrame const& frame)
{
    // DefName := NameOp NameString DataRefObject
    auto path = TRY(NameString::from_reader(reader));

    auto data = TRY(read_data_ref_object(reader, frame));
    auto node = make_ref_counted<NameNode>(data);

    TRY(insert_node(path, frame.node(), node));
    return {};
}

ErrorOr<void> Interpreter::process_def_operation_region(TableReader& reader, ParseFrame const& frame)
{
    // DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen
    auto path = TRY(NameString::from_reader(reader));

    auto region_space = reader.byte();
    auto region_offset = TRY(TRY(read_term_arg(reader, frame)).as_integer());
    auto region_length = TRY(TRY(read_term_arg(reader, frame)).as_integer());

    auto node = make_ref_counted<OperationRegionNode>(region_space, region_offset, region_length);
    TRY(insert_node(path, frame.node(), node));
    return {};
}

ErrorOr<void> Interpreter::process_def_unit_field(TableReader& reader, ParseFrame const& frame, u16 opcode)
{
    // DefCreateByteField  := CreateByteFieldOp SourceBuff ByteIndex NameString
    // DefCreateWordField  := CreateWordFieldOp SourceBuff ByteIndex NameString
    // DefCreateDWordField := CreateDWordFieldOp SourceBuff ByteIndex NameString
    // DefCreateQWordField := CreateQWordFieldOp SourceBuff ByteIndex NameString
    // DefCreateBitField   := CreateBitFieldOp SourceBuff ByteIndex NameString

    auto buffer_ptr = TRY(TRY(read_term_arg(reader, frame)).as_buffer_ptr());
    auto index = TRY(TRY(read_term_arg(reader, frame)).as_integer());
    auto path = TRY(NameString::from_reader(reader));
    warnln("Adding BufferField at {}", TRY(path.to_string()));

    int bit_size = 0;
    switch (opcode) {
    case (u16)Opcode::CreateBitFieldOp:
        bit_size = 1;
        break;
    case (u16)Opcode::CreateByteFieldOp:
        bit_size = 8;
        break;
    case (u16)Opcode::CreateWordFieldOp:
        bit_size = 16;
        break;
    case (u16)Opcode::CreateDWordFieldOp:
        bit_size = 32;
        break;
    case (u16)Opcode::CreateQWordFieldOp:
        bit_size = 64;
        break;
    default:
        return Error::from_string_literal("Invalid buffer field opcode!");
    }
    auto bit_offset = index * (bit_size == 1 ? 1 : 8);

    auto node = make_ref_counted<BufferFieldNode>(buffer_ptr, bit_offset, bit_size);
    TRY(insert_node(path, frame.node(), node));
    return {};
}

ErrorOr<NodeData> Interpreter::read_def_buffer(TableReader& reader, ParseFrame const& frame)
{
    // DefBuffer := BufferOp PkgLength BufferSize ByteList
    auto package_length = reader.package_length();

    auto term_arg = TRY(read_term_arg(reader, frame));
    auto buffer_size = TRY(term_arg.as_integer());

    if (package_length > buffer_size) {
        warnln("[LibACPI] Interpreter::read_def_buffer: Buffer size overrun, package length {} and buffer size {}", package_length, buffer_size);
    }

    // FIXME: Figure out if ignoring package size and just using the buffer size is expected behaviour.
    Vector<u8> data;
    data.resize(buffer_size);
    reader.read_into(buffer_size, data);
    return NodeData(Buffer(data));
}

ErrorOr<NodeData> Interpreter::read_package(TableReader& reader, ParseFrame const& frame, u16 opcode)
{
    auto package_length = reader.package_length();
    size_t num_elements = 0;

    if (opcode == (u16)Opcode::PackageOp) {
        // DefPackage := PackageOp PkgLength NumElements PackageElementList
        // NumElements := ByteData
        num_elements = reader.byte();
    } else if (opcode == (u16)Opcode::VarPackageOp) {
        // DefPackage := PackageOp PkgLength VarNumElements PackageElementList
        auto num_element_term = TRY(read_term_arg(reader, frame));
        num_elements = TRY(num_element_term.as_integer());
    } else {
        return Error::from_string_literal("Non-package opcode");
    }

    (void)package_length;

    Vector<NodeData> package;
    // PackageElementList := Nothing | <packageelement | packageelementlist>
    for (size_t i = 0; i < num_elements; i++) {
        // PackageElement := DataRefObject | NameString
        if (TableReader::is_lead_name_char(reader.peek())) {
            auto path = TRY(NameString::from_reader(reader));
            (void)path;

            return Error::from_string_literal("Object evaluation not yet implemented.");
        }

        auto data_ref_object = TRY(read_data_ref_object(reader, frame));
        package.append(data_ref_object);
    }

    return NodeData(package);
}

ErrorOr<NodeData> Interpreter::read_data_object(TableReader& reader, ParseFrame const& frame, u16 opcode)
{
    (void)frame;

    // DataObject := ComputationalData | DefPackage | DefVarPackage
    auto value = read_computational_data(reader, frame, opcode);
    if (!value.is_error())
        return value.release_value();

    value = read_package(reader, frame, opcode);
    if (!value.is_error())
        return value.release_value();

    return Error::from_string_literal("Unimplemented");
}

ErrorOr<NodeData> Interpreter::read_data_ref_object(TableReader& reader, ParseFrame const& frame)
{
    // DataRefObject := DataObject | ObjectReference
    u16 opcode = reader.opcode();
    auto compute = read_data_object(reader, frame, opcode);
    if (!compute.is_error())
        return compute.release_value();

    // ObjectReference := Integer
    UNIMPLEMENTED_OPCODE("read_data_ref_object", opcode);
}

ErrorOr<RefPtr<MethodNode>> Interpreter::find_method(NameString const& path, RefPtr<Node> const& scope)
{
    auto node = TRY(find_node(path, scope));
    if (node->type() != NodeType::Method) {
        return Error::from_string_view_or_print_error_and_return_errno("Expected method!"sv, EINVAL);
    }
    return static_ptr_cast<MethodNode>(node);
}

ErrorOr<NodeData> Interpreter::read_expression_opcode(TableReader& reader, ParseFrame const& frame, u16 opcode)
{
    if (TableReader::is_lead_name_char(opcode)) {
        // MethodInvocation := NameString TermArgList
        auto path = TRY(NameString::from_reader(reader));
        auto method = TRY(find_method(path, frame.node()));

        ParseFrame frame(method, method->start(), method->end());
        // TermArgList := Nothing | <termarg termarglist>
        for (int i = 0; i < method->arguments(); i++) {
            auto argument = TRY(read_term_arg(reader, frame));
            TRY(frame.set_argument(i, argument));
        }

        push_parse_frame(frame);
    }
    return Error::from_errno(EINVAL);
}

ErrorOr<NodeData> Interpreter::read_term_arg(TableReader& reader, ParseFrame const& frame)
{
    // TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj
    u16 opcode = reader.peek();
    if (!TableReader::is_lead_name_char(opcode)) {
        opcode = reader.byte();
    }

    auto object = read_expression_opcode(reader, frame, opcode);
    if (!object.is_error()) {
        return object.release_value();
    }

    object = read_data_object(reader, frame, opcode);
    if (!object.is_error()) {
        return object.release_value();
    }

    UNIMPLEMENTED_OPCODE("read_term_arg", opcode);
}

ErrorOr<NodeData> Interpreter::read_computational_data(TableReader& reader, ParseFrame const& frame, u16 opcode)
{
    // ComputationalData := ByteConst | WordConst | DWordConst | QWordConst |
    //                      String | ConstObj | RevisionOp | DefBuffer
    // dbgln("[LibACPI] Interpreter::read_computational_data: Opcode {:4X}", opcode);
    switch (static_cast<Opcode>(opcode)) {
    case Opcode::BytePrefix:
        return NodeData(reader.byte());
    case Opcode::WordPrefix:
        return NodeData(reader.word());
    case Opcode::DWordPrefix:
        return NodeData(static_cast<i32>(reader.dword()));
    case Opcode::QWordPrefix:
        return NodeData(static_cast<i64>(reader.qword()));
    case Opcode::StringPrefix:
        return NodeData(TRY(reader.string()));
    case Opcode::ZeroOp:
        return NodeData(static_cast<i64>(0ull));
    case Opcode::OneOp:
        return NodeData(static_cast<i64>(1ull));
    case Opcode::OnesOp:
        return NodeData(static_cast<i64>(-1));
    case Opcode::RevisionOp:
        // TODO: Figure out what this actually returns.
        return NodeData(static_cast<i64>(1ull));
    case Opcode::BufferOp:
        return read_def_buffer(reader, frame);
    default:
        break;
    }

    UNIMPLEMENTED_OPCODE("read_computational_data", opcode);
}

void Interpreter::push_parse_frame(ParseFrame const& frame)
{
    dbgln("[LibACPI] Interpreter::push_parse_frame: Entering parse frame {} with end at {:X}", frame.node()->name().to_string_view(), frame.end());
    m_parse_frames.append(frame);
}

ParseFrame Interpreter::pop_parse_frame()
{
    auto frame = m_parse_frames[m_parse_frames.size() - 1];
    m_parse_frames.remove(m_parse_frames.size() - 1);

    dbgln("[LibACPI] Interpreter::pop_parse_frame: Leaving parse frame {} with end at {:X}", frame.node()->name().to_string_view(), frame.end());
    return frame;
}

ErrorOr<RefPtr<Table>> Interpreter::interpret(u8* buffer, size_t length)
{
    TableReader reader(StringView((char*)buffer, length));

    // AMLCode := DefBlockHeader TermList

    // DefBlockHeader := TableSignature TableLength
    //                   SpecCompliance CheckSum
    //                   OemID OemTableID OemRevision
    //                   CreatorID CreatorRevision

    // TableSignature   := DWordData
    // TableLength      := DWordData
    // SpecCompliance   := ByteData
    // CheckSum         := ByteData
    // OemID            := ByteData(6)::ZeroTerminated
    // OemTableID       := ByteData(8)::ZeroTerminated
    // OemRevision      := DWordData
    // CreatorID        := DWordData
    // CreatorRevision  := DWordData
    auto table_signature = reader.dword();
    auto table_length = reader.dword();
    auto spec_compliance = reader.byte();
    auto checksum = reader.byte();
    auto oem_id = reader.zero_terminated_byte_array<6>();
    auto oem_table_id = reader.zero_terminated_byte_array<8>();
    auto oem_revision = reader.dword();
    auto creator_id = reader.dword();
    auto creator_revision = reader.dword();
    if ((table_length == 0) || (reader.position() < 22)) {
        return Error::from_string_literal("[LibACPI] Reader failure.");
    }

    auto generated_checksum = reader.generate_checksum();
    if (generated_checksum != 0) {
        warnln("[LibACPI] Interpreter::interpret: Checksum incorrect, expected 0 but got {}!", generated_checksum);
        return Error::from_string_literal("Checksum failure.");
    }

    auto table = make_ref_counted<ACPI::Table>();
    table->set_block_header(ACPI::BlockHeader(table_signature, table_length, spec_compliance, checksum, oem_id, oem_table_id, oem_revision, creator_id, creator_revision));

    m_table = table;

    auto root_frame = ParseFrame(m_table->namespace_root(), length, length);
    push_parse_frame(root_frame);

    for (;;) {
        dbgln("[LibACPI] Interpreter::interpret: Reader position: {:04X}", reader.position());
        if (m_parse_frames.is_empty())
            break;
        if (reader.is_eof())
            break;

        auto frame = m_parse_frames[m_parse_frames.size() - 1];
        TRY(read_term(reader, frame));

        // Pop all frames that have run out.
        // Frame variable gets reloaded as read_term may change the current scope,
        // and that scope may be empty.
        do {
            frame = m_parse_frames[m_parse_frames.size() - 1];
            if (reader.position() >= frame.end()) {
                (void)pop_parse_frame();
                reader.set_position(frame.start());
            }
        } while (m_parse_frames.size() != 0 && (reader.position() >= frame.end()));
    }

    warnln("Length: {}, Position: {}, Left over: {}", table_length, reader.position(), table_length - reader.position());
    return m_table;
}

ErrorOr<void> Interpreter::read_term(TableReader& reader, ParseFrame const& frame)
{
    if (TableReader::is_lead_name_char(reader.peek())) {
        auto ns = TRY(NameString::from_reader(reader));
        warnln("[LibACPI] Method invocation {}.", ns.to_string());
        return Error::from_string_literal("Methods not implemented");
    }

    u16 opcode = reader.opcode();

    dbgln("[LibACPI] Interpreter::read_term: Handling opcode {:04X}", opcode);
    switch (static_cast<Opcode>(opcode)) {
    case Opcode::ScopeOp:
        TRY(process_def_scope(reader, frame));
        break;
    case Opcode::DeviceOp:
        TRY(process_def_device(reader, frame));
        break;
    case Opcode::NameOp:
        TRY(process_def_name(reader, frame));
        break;
    case Opcode::OpRegionOp:
        TRY(process_def_operation_region(reader, frame));
        break;
    case Opcode::FieldOp:
        TRY(process_def_field(reader, frame));
        break;
    case Opcode::MethodOp:
        TRY(process_def_method(reader, frame));
        break;
    case Opcode::ProcessorOp:
        TRY(process_def_processor(reader, frame));
        break;
    case Opcode::CreateBitFieldOp:
    case Opcode::CreateByteFieldOp:
    case Opcode::CreateWordFieldOp:
    case Opcode::CreateDWordFieldOp:
    case Opcode::CreateQWordFieldOp:
        TRY(process_def_unit_field(reader, frame, opcode));
        break;
    default:
        UNIMPLEMENTED_OPCODE("read_term", opcode);
    }
    return {};
}

}
