/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibACPI/Interpreter.h>
#include <LibACPI/TableStream.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibMain/Main.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    // FIXME: Unveil rigamarole.
    // FIXME: Multiple commands outside of debugging.
    // FIXME: Interactive mode.
    bool ignore_block_header = false;
    Vector<StringView> input_files;

    Core::ArgsParser args_parser;
    args_parser.set_general_help("ACPI management utility.");
    args_parser.add_option(ignore_block_header, "Treats the input file as not containing the block header.", "ignore-block-header", 'i');
    args_parser.add_positional_argument(input_files, "Files containing AML table bytecode to load.", "input");
    args_parser.parse(arguments);

    if (input_files.is_empty())
        return Error::from_string_literal("Input file is required, use '-' to read from standard input");

    // Invoke interpreter.
    HashMap<u64, ByteBuffer> simulated_memory_buckets;
    ACPI::InterpreterCallbacks callbacks = {
        .on_read_system_memory = [&simulated_memory_buckets](ACPI::IntegerKind kind, u64 address) -> ErrorOr<u64> {
            u64 bucket = address >> 12;
            u64 offset = address & 0xFFF;

            auto buffer = simulated_memory_buckets.get(bucket);
            if (!buffer.has_value()) {
                return 0;
            }

            auto stream = FixedMemoryStream(buffer.release_value().bytes());
            TRY(stream.seek(offset));

            switch (kind) {
            case ACPI::IntegerKind::Byte:
                return stream.read_value<u8>();
            case ACPI::IntegerKind::Word:
                return stream.read_value<u16>();
            case ACPI::IntegerKind::DWord:
                return stream.read_value<u32>();
            case ACPI::IntegerKind::QWord:
                return stream.read_value<u64>();
            }
            return 0;
        },
        .on_write_system_memory = [&simulated_memory_buckets](ACPI::IntegerKind kind, u64 address, u64 value) -> ErrorOr<void> {
            u64 bucket = address >> 12;
            u64 offset = address & 0xFFF;

            auto buffer = simulated_memory_buckets.get(bucket);
            ByteBuffer buffer_value;
            if (!buffer.has_value()) {
                ByteBuffer bucket_buffer;
                TRY(bucket_buffer.try_resize(4096));
                simulated_memory_buckets.set(bucket, bucket_buffer);
                buffer_value = bucket_buffer;
            } else {
                buffer_value = buffer.release_value();
            }

            auto stream = FixedMemoryStream(buffer_value.bytes());
            TRY(stream.seek(offset));

            switch (kind) {
            case ACPI::IntegerKind::Byte:
                TRY(stream.write_value<u8>(value));
                break;
            case ACPI::IntegerKind::Word:
                TRY(stream.write_value<u16>(value));
                break;
            case ACPI::IntegerKind::DWord:
                TRY(stream.write_value<u32>(value));
                break;
            case ACPI::IntegerKind::QWord:
                TRY(stream.write_value<u64>(value));
                break;
            }
            return {};
        },
    };

    ACPI::Interpreter interpreter(callbacks);
    for (size_t i = 0; i < input_files.size(); i++) {
        auto input = input_files[i];
        auto result = Core::File::open_file_or_standard_stream(input, Core::File::OpenMode::Read);
        if (result.is_error()) {
            warnln("Failed to open {}: {}", input, result.release_error());
            return 1;
        }

        auto input_file = result.release_value();
        auto table_data = TRY(input_file->read_until_eof());

        ACPI::TableStream stream(table_data.span());
        TRY(interpreter.load(stream, ignore_block_header));
    }

    interpreter.print_namespace();
    return 0;
}
