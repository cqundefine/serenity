/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once
#include <AK/Array.h>

namespace ACPI {

class BlockHeader {
public:
    BlockHeader() = default;
    BlockHeader(u32 signature, u32 length, u32 spec_compliance, u8 checksum, AK::Array<u8, 6> oem_id, AK::Array<u8, 8> oem_table_id, u32 oem_revision, u32 creator_id, u32 creator_revision)
        : m_signature(signature)
        , m_length(length)
        , m_spec_compliance(spec_compliance)
        , m_checksum(checksum)
        , m_oem_id(oem_id)
        , m_oem_table_id(oem_table_id)
        , m_oem_revision(oem_revision)
        , m_creator_id(creator_id)
        , m_creator_revision(creator_revision)
    {
    }

    u32 signature() const { return m_signature; }
    u32 length() const { return m_length; }
    u32 spec_compliance() const { return m_spec_compliance; }
    u8 checksum() const { return m_checksum; }
    AK::Array<u8, 6> oem_id() const { return m_oem_id; }
    AK::Array<u8, 8> oem_table_id() const { return m_oem_table_id; }
    u32 oem_revision() const { return m_oem_revision; }
    u32 creator_id() const { return m_creator_id; }
    u32 creator_revision() const { return m_creator_revision; }

protected:
    u32 m_signature;
    u32 m_length;
    u32 m_spec_compliance;
    u8 m_checksum;
    AK::Array<u8, 6> m_oem_id;
    AK::Array<u8, 8> m_oem_table_id;
    u32 m_oem_revision;
    u32 m_creator_id;
    u32 m_creator_revision;
};

}
