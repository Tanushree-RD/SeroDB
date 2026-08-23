#pragma once

#include "serodb/row.hpp"

namespace serodb {

enum class StatementType { insert, select };

enum class ParseResult {
    success,
    unrecognized_statement,
    syntax_error,
    invalid_id,
    username_too_long,
    email_too_long,
};

struct Statement {
    StatementType type{};
    Row row;
};

} // namespace serodb
