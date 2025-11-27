/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "OpcodeFrame.h"
#include "Definitions.h"
#include "TableStream.h"

namespace ACPI {

// Operand definitions.
static Array<MetaOpcode, 3> scope_operands = { MetaOpcode::PackageLength, MetaOpcode::NameString, MetaOpcode::End };
static Array<MetaOpcode, 3> name_operands = { MetaOpcode::NameString, MetaOpcode::DataRefObject, MetaOpcode::End };

static Array<MetaOpcode, 2> byte_const_operands = { MetaOpcode::ByteData, MetaOpcode::End };
static Array<MetaOpcode, 2> word_const_operands = { MetaOpcode::WordData, MetaOpcode::End };
static Array<MetaOpcode, 2> dword_const_operands = { MetaOpcode::DWordData, MetaOpcode::End };
static Array<MetaOpcode, 2> qword_const_operands = { MetaOpcode::QWordData, MetaOpcode::End };

static Array<MetaOpcode, 5> op_region_operands = { MetaOpcode::NameString, MetaOpcode::ByteData, MetaOpcode::TermArg, MetaOpcode::TermArg, MetaOpcode::End };
static Array<MetaOpcode, 7> field_operands = {
    MetaOpcode::MarkAsCount, MetaOpcode::PackageLength,
    MetaOpcode::NameString,
    MetaOpcode::ByteData,
    MetaOpcode::RepeatNextBytes, MetaOpcode::FieldElement,
    MetaOpcode::End
};
static Array<MetaOpcode, 6> package_operands = {
    MetaOpcode::PackageLength,
    MetaOpcode::MarkAsCount, MetaOpcode::ByteData,
    MetaOpcode::RepeatNextCount, MetaOpcode::PackageElement,
    MetaOpcode::End
};

static Array<MetaOpcode, 6> buffer_operands = {
    MetaOpcode::PackageLength,
    MetaOpcode::MarkAsCount, MetaOpcode::TermArg,
    MetaOpcode::ByteBuffer,
    MetaOpcode::End
};

static Array<MetaOpcode, 3> alias_operands = {
    MetaOpcode::NameString,
    MetaOpcode::NameString,
    MetaOpcode::End
};

static Array<MetaOpcode, 3> device_operands = {
    MetaOpcode::PackageLength,
    MetaOpcode::NameString,
    MetaOpcode::End
};

static Array<MetaOpcode, 4> method_operands = {
    MetaOpcode::PackageLength,
    MetaOpcode::NameString,
    MetaOpcode::ByteData,
    MetaOpcode::End
};

static Array<MetaOpcode, 4> shift_left_operands = {
    MetaOpcode::TermArg,
    MetaOpcode::TermArg,
    MetaOpcode::Target,
    MetaOpcode::End
};
static Array<MetaOpcode, 2> string_const_operands = {
    MetaOpcode::String,
    MetaOpcode::End
};
static Array<MetaOpcode, 1> empty_operands = {
    MetaOpcode::End
};

static Array<MetaOpcode, 4> create_unit_field_operands = {
    MetaOpcode::TermArg,
    MetaOpcode::TermArg,
    MetaOpcode::NameString,
    MetaOpcode::End
};

static Array<MetaOpcode, 3> mutex_operands = {
    MetaOpcode::NameString,
    MetaOpcode::ByteData,
    MetaOpcode::End
};

static Array<MetaOpcode, 6> processor_operands = {
    MetaOpcode::PackageLength,
    MetaOpcode::NameString,
    MetaOpcode::ByteData,
    MetaOpcode::DWordData,
    MetaOpcode::ByteData,
    MetaOpcode::End
};

static Array<MetaOpcode, 3> thermal_zone_operands = {
    MetaOpcode::PackageLength,
    MetaOpcode::NameString,
    MetaOpcode::End
};

static ErrorOr<Span<MetaOpcode>> get_opcode_operand_template(u16 opcode)
{
    switch (opcode) {
    case (u16)Opcode::BytePrefix:
        return byte_const_operands.span();
    case (u16)Opcode::WordPrefix:
        return word_const_operands.span();
    case (u16)Opcode::DWordPrefix:
        return dword_const_operands.span();
    case (u16)Opcode::QWordPrefix:
        return qword_const_operands.span();
    case (u16)Opcode::StringPrefix:
        return string_const_operands.span();

    case (u16)Opcode::ScopeOp:
        return scope_operands.span();
    case (u16)Opcode::NameOp:
        return name_operands.span();

    case (u16)Opcode::ProcessorOp:
        return processor_operands.span();
    case (u16)Opcode::OpRegionOp:
        return op_region_operands.span();
    case (u16)Opcode::FieldOp:
        return field_operands.span();
    case (u16)Opcode::PackageOp:
        return package_operands.span();
    case (u16)Opcode::BufferOp:
        return buffer_operands.span();
    case (u16)Opcode::AliasOp:
        return alias_operands.span();
    case (u16)Opcode::DeviceOp:
        return device_operands.span();
    case (u16)Opcode::MethodOp:
        return method_operands.span();
    case (u16)Opcode::MutexOp:
        return mutex_operands.span();
    case (u16)Opcode::ThermalZoneOp:
        return thermal_zone_operands.span();

    case (u16)Opcode::CreateByteFieldOp:
    case (u16)Opcode::CreateWordFieldOp:
    case (u16)Opcode::CreateDWordFieldOp:
    case (u16)Opcode::CreateQWordFieldOp:
        return create_unit_field_operands.span();

    case (u16)Opcode::ShiftLeftOp:
        return shift_left_operands.span();

    case (u16)Opcode::ZeroOp:
    case (u16)Opcode::OneOp:
    case (u16)Opcode::OnesOp:
    case (u16)MetaOpcode::End:
        return empty_operands.span();
    default:
        warnln("[LibACPI] get_opcode_operand_template: Unknown opcode template: {:04X}!", opcode);
        return Error::from_string_view_or_print_error_and_return_errno("Unknown opcode template!"sv, EINVAL);
    }
}

// Filter functions.
static bool namespace_modifier_object_filter(u16 opcode)
{
    // NameSpaceModifierObj := DefAlias | DefName | DefScope
    return (
        opcode == (u16)Opcode::AliasOp || opcode == (u16)Opcode::NameOp || opcode == (u16)Opcode::ScopeOp);
}

static bool named_object_filter(u16 opcode)
{
    // NamedObj := DefBankField | DefCreateBitField | DefCreateByteField | DefCreateDWordField | DefCreateField | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal | DefOpRegion | DefPowerRes | DefThermalZone

    // FieldOp missing from 6.6 docs, but given the presence of the DefXYZField
    // opcodes here, I'm adding it in here.
    // DeviceOp is also missing. Adding it here too.
    // MutexOp likewise missing. It's named and creates an object, so here it be.

    // ProcessorOp found here in 6.3 docs.
    // FIXME: Add support for missing opcodes.
    return (
        opcode == (u16)Opcode::CreateBitFieldOp || opcode == (u16)Opcode::CreateByteFieldOp || opcode == (u16)Opcode::CreateWordFieldOp || opcode == (u16)Opcode::CreateDWordFieldOp || opcode == (u16)Opcode::CreateQWordFieldOp || opcode == (u16)Opcode::FieldOp || opcode == (u16)Opcode::MutexOp || opcode == (u16)Opcode::DeviceOp || opcode == (u16)Opcode::MethodOp || opcode == (u16)Opcode::ProcessorOp || opcode == (u16)Opcode::ThermalZoneOp || opcode == (u16)Opcode::OpRegionOp);
}

static bool object_filter(u16 opcode)
{
    // Object := NameSpaceModifierObj | NamedObj
    return namespace_modifier_object_filter(opcode) || named_object_filter(opcode);
}

static bool statement_filter(u16 opcode)
{
    // StatementOpcode := DefBreak | DefBreakPoint | DefContinue | DefFatal | DefIfElse | DefNoop | DefNotify | DefRelease | DefReset | DefReturn | DefSignal | DefSleep | DefStall | DefWhile

    // FIXME: Add support for statement opcodes.
    (void)opcode;
    return false;
}

static bool expression_filter(u16 opcode)
{
    // ExpressionOpcode := DefAcquire | DefAdd | DefAnd | DefBuffer | DefConcat | DefConcatRes | DefCondRefOf | DefCopyObject | DefDecrement | DefDerefOf | DefDivide | DefFindSetLeftBit | DefFindSetRightBit | DefFromBCD | DefIncrement | DefIndex | DefLAnd | DefLEqual | DefLGreater | DefLGreaterEqual | DefLLess | DefLLessEqual | DefMid | DefLNot | DefLNotEqual | DefLoadTable | DefLOr | DefMatch | DefMod | DefMultiply | DefNAnd | DefNOr | DefNot | DefObjectType | DefOr | DefPackage | DefVarPackage | DefRefOf | DefShiftLeft | DefShiftRight | DefSizeOf | DefStore | DefSubtract | DefTimer | DefToBCD | DefToBuffer | DefToDecimalString | DefToHexString | DefToInteger | DefToString | DefWait | DefXOr | MethodInvocation

    // FIXME: Add support for expression opcodes.
    (void)opcode;
    return false;
}

static bool const_object_filter(u16 opcode)
{
    // ConstObj := ZeroOp | OneOp | OnesOp
    return (
        opcode == (u16)Opcode::ZeroOp || opcode == (u16)Opcode::OneOp || opcode == (u16)Opcode::OnesOp);
}

static bool computational_data_filter(u16 opcode)
{
    // ComputationalData := ByteConst | WordConst | DWordConst | QWordConst | String | ConstObj | RevisionOp | DefBuffer
    return (
        opcode == (u16)Opcode::BytePrefix || opcode == (u16)Opcode::WordPrefix || opcode == (u16)Opcode::DWordPrefix || opcode == (u16)Opcode::QWordPrefix || opcode == (u16)Opcode::StringPrefix || opcode == (u16)Opcode::RevisionOp || const_object_filter(opcode) || opcode == (u16)Opcode::BufferOp);
}

static bool data_object_filter(u16 opcode)
{
    // DataObject := ComputationalData | DefPackage | DefVarPackage
    return computational_data_filter(opcode) || (opcode == (u16)Opcode::PackageOp) || (opcode == (u16)Opcode::VarPackageOp);
}

static bool data_ref_object_filter(u16 opcode)
{
    // DataRefObject := DataObject | ObjectReference
    // FIXME: Add support for ObjectReference.
    return data_object_filter(opcode);
}

static bool package_element_filter(u16 opcode)
{
    // PackageElement := DataRefObject | NameString
    return data_ref_object_filter(opcode) || TableStream::is_namestring_start(opcode);
}

static bool term_filter(u16 opcode)
{
    return (object_filter(opcode) || statement_filter(opcode) || expression_filter(opcode));
}

static bool arg_obj_filter(u16 opcode)
{
    return (
        opcode == (u16)Opcode::Arg0Op || opcode == (u16)Opcode::Arg1Op || opcode == (u16)Opcode::Arg2Op || opcode == (u16)Opcode::Arg3Op || opcode == (u16)Opcode::Arg4Op || opcode == (u16)Opcode::Arg5Op || opcode == (u16)Opcode::Arg6Op);
}

static bool local_obj_filter(u16 opcode)
{
    return (
        opcode == (u16)Opcode::Local0Op || opcode == (u16)Opcode::Local1Op || opcode == (u16)Opcode::Local2Op || opcode == (u16)Opcode::Local3Op || opcode == (u16)Opcode::Local4Op || opcode == (u16)Opcode::Local5Op || opcode == (u16)Opcode::Local6Op || opcode == (u16)Opcode::Local7Op);
}

static bool simple_name_filter(u16 opcode)
{
    // SimpleName := NameString | ArgObj | LocalObj
    return (TableStream::is_lead_name_char(opcode) || arg_obj_filter(opcode) || local_obj_filter(opcode));
}

static bool debug_obj_filter(u16 opcode)
{
    // DebugObj := DebugOp
    return opcode == (u16)Opcode::DebugOp;
}

static bool reference_type_opcode_filter(u16 opcode)
{
    // ReferenceTypeOpcode := DefRefOf | DefDerefOf | DefIndex | UserTermObj
    // FIXME: Implement whatever this is.
    (void)opcode;
    return false;
}

static bool super_name_filter(u16 opcode)
{
    // SuperName := SimpleName | DebugObj | ReferenceTypeOpcode
    return simple_name_filter(opcode) || debug_obj_filter(opcode) || reference_type_opcode_filter(opcode);
}

static bool target_filter(u16 opcode)
{
    // Target := SuperName | NullName
    return super_name_filter(opcode) || (opcode == 0x00);
}

MetaOpcode OpcodeFrame::peek_next_operand()
{
    auto operand = m_operand_template[m_operand_index];
    if (operand == MetaOpcode::MarkAsCount || operand == MetaOpcode::RepeatNextCount || operand == MetaOpcode::RepeatNextBytes) {
        operand = m_operand_template[m_operand_index + 1];
    }
    return operand;
}

bool OpcodeFrame::filter_opcode_for_next_operand(u16 opcode)
{
    auto pending_operand = peek_next_operand();
    // dbgln("Filtering opcode {:04X} via operand {:04X}", opcode, (u16) pending_operand);
    switch (pending_operand) {
    case MetaOpcode::NameString:
    case MetaOpcode::PackageLength:
    case MetaOpcode::ByteData:
    case MetaOpcode::WordData:
    case MetaOpcode::DWordData:
    case MetaOpcode::QWordData:
    case MetaOpcode::String:
    case MetaOpcode::MarkAsCount:
    case MetaOpcode::RepeatNextCount:
    case MetaOpcode::RepeatNextBytes:
    case MetaOpcode::ByteBuffer:
        return false;

    case MetaOpcode::FieldElement:
    case MetaOpcode::TermArg:
    case MetaOpcode::DataObject:
    case MetaOpcode::ObjectReference:
    case MetaOpcode::SimpleName:
    case MetaOpcode::NullName:

    case MetaOpcode::ComputationalData:
        // FIXME: Implement correct filter for every operand type.
        return true;

    case MetaOpcode::SuperName:
        return super_name_filter(opcode);

    case MetaOpcode::Target:
        return target_filter(opcode);
    case MetaOpcode::DataRefObject:
        return data_ref_object_filter(opcode);
    case MetaOpcode::PackageElement:
        return package_element_filter(opcode);
    case MetaOpcode::End:
        return term_filter(opcode);
    }
    return false;
}

ErrorOr<MetaOpcode> OpcodeFrame::begin_operand()
{
    auto operand = m_operand_template[m_operand_index];
    if (operand == MetaOpcode::MarkAsCount) {
        // Save the index of the next loaded argument.
        m_counter_value_index = size();
    } else if (operand == MetaOpcode::RepeatNextCount || operand == MetaOpcode::RepeatNextBytes || operand == MetaOpcode::ByteBuffer) {
        // Initialize counter and return the next operand.
        if (!m_counter.has_value()) {
            auto count = TRY(at(m_counter_value_index).as_integer());
            if (count == 0) {
                m_counter = {};
                m_counter_end = 0;
                m_operand_index += (operand != MetaOpcode::ByteBuffer) ? 2 : 1;
            } else {
                m_counter = count;
                m_counter_end = count;
            }
        }
    }
    if (operand == MetaOpcode::PackageLength) {
        m_package_length_index = size();
    }

    // Automatically returns either the current operand or the operand right after as needed.
    return peek_next_operand();
}

ErrorOr<void> OpcodeFrame::finish_operand(size_t current_offset)
{
    auto operand = m_operand_template[m_operand_index];
    if (operand == MetaOpcode::MarkAsCount) {
        // Skip over argument.
        m_operand_index += 2;
        return {};
    }

    if (operand == MetaOpcode::RepeatNextCount) {
        // Counter check is with 1, because when the counter hits 0,
        // we need begin_operand() to already be at the next operand.
        // FIXME: Which is nicer, comparing here with 1 or modifying the count in begin_operand?
        if (m_counter.value() == 1) {
            // Release the counter value and move to the next operand.
            (void)m_counter.release_value();
            m_operand_index += 2;
            return {};
        }

        m_counter.value()--;
        return {};
    }

    if (operand == MetaOpcode::RepeatNextBytes) {
        if (current_offset > m_start + m_counter.value()) {
            // Release the counter value and move to the next operand.
            (void)m_counter.release_value();
            m_operand_index += 2;
            return {};
        }

        // Counter is effectively handled outside.
        return {};
    }
    m_operand_index++;
    return {};
}

ErrorOr<OpcodeFrame> OpcodeFrame::from_opcode(u16 opcode, size_t start)
{
    auto operand_template = TRY(get_opcode_operand_template(opcode));
    return OpcodeFrame(opcode, operand_template, start);
}

}
