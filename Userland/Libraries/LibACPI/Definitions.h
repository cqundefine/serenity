/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

enum class Prefix : u8 {
    ExtOpPrefix = 0x5B,
    DualNamePrefix = 0x2E,
    MultiNamePrefix = 0x2F,
};

enum class Opcode : u16 {
    ZeroOp = 0x0000,
    OneOp = 0x0001,
    OnesOp = 0x00FF,

    NameOp = 0x0008,
    ScopeOp = 0x0010,
    BufferOp = 0x0011,
    PackageOp = 0x0012,
    VarPackageOp = 0x0013,
    MethodOp = 0x0014,

    CreateDWordFieldOp = 0x8A,
    CreateWordFieldOp = 0x8B,
    CreateByteFieldOp = 0x8C,
    CreateBitFieldOp = 0x8D,
    CreateQWordFieldOp = 0x8F,

    RevisionOp = 0x5B30,

    Local0Op = 0x0060,
    Local1Op = 0x0061,
    Local2Op = 0x0062,
    Local3Op = 0x0063,
    Local4Op = 0x0064,
    Local5Op = 0x0065,
    Local6Op = 0x0066,
    Local7Op = 0x0067,
    Arg0Op = 0x0068,
    Arg1Op = 0x0069,
    Arg2Op = 0x006A,
    Arg3Op = 0x006B,
    Arg4Op = 0x006C,
    Arg5Op = 0x006D,
    Arg6Op = 0x006E,

    OpRegionOp = 0x5B80,
    FieldOp = 0x5B81,
    DeviceOp = 0x5B82,
    ProcessorOp = 0x5B83,

    // Inside opcodes enum for ease of interpretation
    BytePrefix = 0x0A,
    WordPrefix = 0x0B,
    DWordPrefix = 0x0C,
    QWordPrefix = 0x0E,
    StringPrefix = 0x0D,
};

enum class Local : u8 {
    Zero = 0x60,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven
};

enum class Argument : u8 {
    Zero = 0x68,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
};
