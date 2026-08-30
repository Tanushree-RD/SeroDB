#pragma once

/**
 * Parser.hpp — Command parsing for SeroDB.
 *
 * The Parser converts raw input strings into either:
 *   • A MetaCommand  — dot‑commands like .exit, .help, .stats
 *   • A Statement    — SQL‑like commands like INSERT and SELECT
 *
 * It validates syntax, field lengths, and value ranges before the Executor
 * ever sees the command.  This mirrors the sqlite3_prepare_v2() step in
 * SQLite where SQL text is compiled into a prepared statement.
 */

#include "serodb/Statement.hpp"

#include <string>

namespace serodb {

class Parser {
public:
    /// Try to parse `input` as a meta‑command (starts with '.').
    /// On success, `outCommand` is filled and MetaCommandResult::success
    /// is returned.
    MetaCommandResult parse_meta_command(const std::string& input,
                                         MetaCommand& outCommand) const;

    /// Parse `input` as an SQL‑like statement (INSERT or SELECT).
    /// Equivalent to sqlite3_prepare — called "prepareStatement" in the
    /// project spec.
    ParseResult prepareStatement(const std::string& input,
                                  Statement& outStatement) const;

private:
    bool parse_id(const std::string& token, std::uint32_t& id) const;
};

} // namespace serodb
