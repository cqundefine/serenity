/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 * Copyright (c) 2026, Undefine <undefine@undefine.pl>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NodeData.h"

namespace ACPI {

ErrorOr<i64> NodeData::as_integer()
{
    auto resolved = maybe_resolve_reference();

    if (resolved->m_type == NodeData::Type::Integer)
        return resolved->m_data.get<i64>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to integer!"sv, EINVAL);
}
ErrorOr<NameString> NodeData::as_namestring()
{
    auto resolved = maybe_resolve_reference();

    if (resolved->m_type == NodeData::Type::NameString)
        return resolved->m_data.get<NameString>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to namestring!"sv, EINVAL);
}

ErrorOr<ByteBuffer> NodeData::as_string_raw()
{
    auto resolved = maybe_resolve_reference();

    if (resolved->m_type == NodeData::Type::String)
        return resolved->m_data.get<ByteBuffer>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to string!"sv, EINVAL);
}

ErrorOr<StringView> NodeData::as_string_view()
{
    auto resolved = maybe_resolve_reference();

    if (resolved->m_type != NodeData::Type::String)
        return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to string!"sv, EINVAL);

    auto buffer = resolved->m_data.get<ByteBuffer>();
    return StringView(buffer.bytes());
}

ErrorOr<Buffer*> NodeData::as_buffer_ptr()
{
    auto resolved = maybe_resolve_reference();

    if (resolved->m_type == NodeData::Type::Buffer)
        return resolved->m_data.get_pointer<Buffer>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to buffer!"sv, EINVAL);
}

ErrorOr<Package*> NodeData::as_package_ptr()
{
    auto resolved = maybe_resolve_reference();

    if (resolved->m_type == NodeData::Type::Package)
        return resolved->m_data.get_pointer<Package>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to package!"sv, EINVAL);
}

ErrorOr<NodeData*> NodeData::as_reference()
{
    if (m_type == NodeData::Type::Reference)
        return m_data.get<NodeData*>();
    return Error::from_string_view_or_print_error_and_return_errno("Can not cast given type to reference!"sv, EINVAL);
}

NodeData* NodeData::maybe_resolve_reference()
{
    if (m_type == NodeData::Type::Reference)
        return m_data.get<NodeData*>();
    return this;
}

}
