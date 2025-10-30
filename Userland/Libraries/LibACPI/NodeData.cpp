/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NodeData.h"

namespace ACPI {

ErrorOr<i64> NodeData::as_integer()
{
    switch (m_type) {
    case NodeData::Type::Byte:
        return static_cast<i64>(m_data.get<i8>());
    case NodeData::Type::Word:
        return static_cast<i64>(m_data.get<i16>());
    case NodeData::Type::DWord:
        return static_cast<i64>(m_data.get<i32>());
    case NodeData::Type::QWord:
        return static_cast<i64>(m_data.get<i64>());
    default:
        return Error::from_string_literal("Unimplemented cast!");
    }
}

}
