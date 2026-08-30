#pragma once

/**
 * Executor.hpp — Statement execution engine for SeroDB.
 *
 * The Executor sits between the Parser and the Table.  It takes a parsed
 * Statement and carries out the corresponding operation (INSERT or SELECT),
 * writing human‑readable output to the supplied stream.
 *
 * This separation mirrors the sqlite3_step() / sqlite3VdbeExec() boundary
 * in SQLite, where the virtual machine executes a prepared statement against
 * the B‑Tree layer.  Our version is much simpler — no bytecode, just a
 * switch on the statement type.
 */

#include "serodb/Statement.hpp"
#include "serodb/Table.hpp"

#include <ostream>

namespace serodb {

/// Outcome of executing a statement.
enum class ExecuteResult {
    success,
    table_full,
    error
};

class Executor {
public:
    /// Execute `stmt` against `table`, writing output to `out`.
    static ExecuteResult execute(const Statement& stmt,
                                Table& table,
                                std::ostream& out);

private:
    static ExecuteResult execute_insert(const Statement& stmt,
                                        Table& table,
                                        std::ostream& out);

    static ExecuteResult execute_select(const Statement& stmt,
                                         Table& table,
                                         std::ostream& out);
};

} // namespace serodb
