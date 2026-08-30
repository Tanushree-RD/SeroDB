#include "serodb/Executor.hpp"

#include "serodb/Cursor.hpp"
#include "serodb/Statement.hpp"
#include "serodb/Table.hpp"
#include "serodb/row.hpp"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace serodb {

// -----------------------------------------------------------------------
// Public dispatch
// -----------------------------------------------------------------------

ExecuteResult Executor::execute(const Statement& stmt,
                                Table& table,
                                std::ostream& out)
{
    switch (stmt.type) {
    case StatementType::Insert:
        return execute_insert(stmt, table, out);
    case StatementType::Select:
        return execute_select(stmt, table, out);
    }
    return ExecuteResult::error; // unreachable, but silences warnings
}

// -----------------------------------------------------------------------
// INSERT
// -----------------------------------------------------------------------

ExecuteResult Executor::execute_insert(const Statement& stmt,
                                        Table& table,
                                        std::ostream& out)
{
    try {
        table.insert(stmt.row);
        out << "Executed.\n";
        return ExecuteResult::success;
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        if (msg.find("full") != std::string::npos) {
            out << "Error: Table full.\n";
            return ExecuteResult::table_full;
        }
        out << "Error: " << msg << "\n";
        return ExecuteResult::error;
    }
}

// -----------------------------------------------------------------------
// SELECT  — formatted table output using a Cursor
// -----------------------------------------------------------------------

ExecuteResult Executor::execute_select(const Statement& /*stmt*/,
                                        Table& table,
                                        std::ostream& out)
{
    // First pass: collect all rows so we can compute column widths.
    std::vector<Row> rows;
    rows.reserve(table.row_count());

    for (auto cursor = Cursor::table_start(table); !cursor.end(); cursor.advance()) {
        rows.push_back(cursor.read());
    }

    // Compute dynamic column widths.
    constexpr std::size_t min_id_width = 2;
    constexpr std::size_t min_username_width = 8;
    constexpr std::size_t min_email_width = 5;

    std::size_t id_width       = min_id_width;
    std::size_t username_width = min_username_width;
    std::size_t email_width    = min_email_width;

    for (const auto& row : rows) {
        id_width       = std::max(id_width, std::to_string(row.id).size());
        username_width = std::max(username_width, row.username.size());
        email_width    = std::max(email_width, row.email.size());
    }

    // Header line.
    out << std::left
        << std::setw(static_cast<int>(id_width))       << "id"       << " | "
        << std::setw(static_cast<int>(username_width))  << "username" << " | "
        << std::setw(static_cast<int>(email_width))     << "email"    << '\n';

    // Separator.
    out << std::string(id_width, '-') << "-+-"
        << std::string(username_width, '-') << "-+-"
        << std::string(email_width, '-') << '\n';

    // Data rows.
    for (const auto& row : rows) {
        out << std::left
            << std::setw(static_cast<int>(id_width))       << row.id       << " | "
            << std::setw(static_cast<int>(username_width))  << row.username << " | "
            << std::setw(static_cast<int>(email_width))     << row.email    << '\n';
    }

    out << rows.size() << " row(s).\n";

    return ExecuteResult::success;
}

} // namespace serodb
