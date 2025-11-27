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
    case NodeData::Type::Integer:
        return static_cast<i64>(m_data.get<i64>());
    default:
        return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to integer!"sv, EINVAL);
    }
}

ErrorOr<Buffer*> NodeData::as_buffer_ptr()
{
    if (m_type == NodeData::Type::Buffer)
        return m_data.get_pointer<Buffer>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to buffer!"sv, EINVAL);
}
ErrorOr<NameString> NodeData::as_namestring()
{
    if (m_type == NodeData::Type::NameString)
        return m_data.get<NameString>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to namestring!"sv, EINVAL);
}

ErrorOr<ByteBuffer> NodeData::as_string_raw()
{
    if (m_type != NodeData::Type::String)
        return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to string!"sv, EINVAL);

    return m_data.get<ByteBuffer>();
}
ErrorOr<StringView> NodeData::as_string_view()
{
    if (m_type != NodeData::Type::String)
        return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to string!"sv, EINVAL);

    auto buffer = m_data.get<ByteBuffer>();
    return StringView(buffer.bytes());
}

}
