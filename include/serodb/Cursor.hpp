#pragma once

/**
 * Cursor.hpp — Forward iterator over Table rows.
 *
 * A Cursor points to a specific row within a Table and supports forward‑only
 * traversal.  It is the primary mechanism that higher layers (Executor, REPL)
 * use to scan or read rows — they never index into the table directly.
 *
 * Two factory functions create cursors at well‑known positions:
 *   • table_start()  — first row (for sequential scans / SELECT).
 *   • table_end()    — one past the last row (the append position for INSERT).
 *
 * This mirrors the sqlite3BtreeFirst / sqlite3BtreeNext API.
 */

#include "serodb/Table.hpp"
#include "serodb/row.hpp"

#include <cstddef>

namespace serodb {

class Cursor {
public:
    /// Create a cursor pointing to the first row of `table`.
    static Cursor table_start(Table& table);

    /// Create a cursor pointing past the last row (append position).
    static Cursor table_end(Table& table);

    /// Advance the cursor to the next row.
    void advance();

    /// Return a raw pointer to the current row's serialized bytes inside
    /// its page buffer.  Useful for in‑place writes.
    char* value();

    /// Deserialize and return the row at the current position.
    Row read() const;

    /// Write (serialize) `row` at the current cursor position.
    void write(const Row& row);

    /// True when the cursor has moved past the last row.
    bool end() const noexcept { return end_of_table_; }

    /// The logical row number the cursor currently points to.
    std::size_t row_num() const noexcept { return row_num_; }

private:
    Cursor(Table& table, std::size_t row_num, bool is_end);

    Table& table_;
    std::size_t row_num_;
    bool end_of_table_;
};

} // namespace serodb
