#pragma once

/**
 * Statement.hpp — Parsed command representations for SeroDB.
 *
 * A Statement is the output of the Parser and the input to the Executor.
 * It captures what operation to perform and, for INSERT, the row data.
 *
 * Meta‑commands (dot‑commands like .exit, .help) are handled separately
 * via MetaCommand / MetaCommandResult so that the REPL can process them
 * before entering the prepare‑execute pipeline.
 */

#include "serodb/row.hpp"

namespace serodb {

// -----------------------------------------------------------------------
// SQL‑like statements
// -----------------------------------------------------------------------

enum class StatementType {
    Insert,
    Select
};

/// Outcome of parsing a statement string.
enum class ParseResult {
    success,
    unrecognized_statement,
    syntax_error,
    invalid_id,
    username_too_long,
    email_too_long,
};

/// A parsed statement ready for execution.
struct Statement {
    StatementType type{};
    Row row; // populated only for Insert statements
};

// -----------------------------------------------------------------------
// Meta‑commands  (dot‑commands handled by the REPL, not the executor)
// -----------------------------------------------------------------------

enum class MetaCommand {
    Exit,
    Help,
    Tables,
    Constants,
    Stats
};

enum class MetaCommandResult {
    success,
    unrecognized_command
};

} // namespace serodb
