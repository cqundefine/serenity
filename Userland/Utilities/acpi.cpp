/*
 * Copyright (c) 2025, RatcheT2497 <ratchetnumbers@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/ArgsParser.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <LibMain/Main.h>
#include <LibACPI/Interpreter.h>

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    // FIXME: Unveil rigamarole.
    // FIXME: Multiple commands outside of debugging.
    // FIXME: Interactive mode.
    StringView input {};

    Core::ArgsParser args_parser;
    args_parser.set_general_help("ACPI management utility.");
    args_parser.add_positional_argument(input, "AML table bytecode to examine.", "input");
    args_parser.parse(arguments);

    if (input.is_empty())
        return Error::from_string_literal("Input file is required, use '-' to read from standard input");

    auto result = Core::File::open_file_or_standard_stream(input, Core::File::OpenMode::Read);
    if (result.is_error())
    {
        warnln("Failed to open {}: {}", input, result.release_error());
        return 1;
    }

    auto input_file = result.release_value();
    auto table_data = TRY(input_file->read_until_eof());

    // Invoke interpreter.
    ACPI::Interpreter interpreter;
    auto table = TRY(interpreter.interpret(table_data.data(), table_data.size()));

    table->print_namespace();
    return 0;
}
