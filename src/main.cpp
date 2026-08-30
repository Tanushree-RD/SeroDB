/**
 * main.cpp — SeroDB REPL (Read‑Eval‑Print Loop).
 *
 * The REPL is the outermost layer of the database engine.  It:
 *   1. Opens a Table (which internally creates a Pager).
 *   2. Reads user input line by line.
 *   3. Dispatches meta‑commands (dot‑commands) directly.
 *   4. Parses SQL‑like statements and hands them to the Executor.
 *   5. Closes the table cleanly on exit.
 *
 * This mirrors the sqlite3_shell read‑eval loop in the SQLite CLI.
 */

#include "serodb/Constants.hpp"
#include "serodb/Executor.hpp"
#include "serodb/Parser.hpp"
#include "serodb/Statement.hpp"
#include "serodb/Table.hpp"

#include <iostream>
#include <string>

namespace {

void print_prompt()
{
    std::cout << "serodb > ";
}

void print_parse_error(serodb::ParseResult result)
{
    switch (result) {
    case serodb::ParseResult::unrecognized_statement:
        std::cout << "Unrecognized statement.\n";
        break;
    case serodb::ParseResult::syntax_error:
        std::cout << "Syntax error.\n";
        break;
    case serodb::ParseResult::invalid_id:
        std::cout << "Invalid id. Expected an unsigned 32-bit integer.\n";
        break;
    case serodb::ParseResult::username_too_long:
        std::cout << "Username is too long.\n";
        break;
    case serodb::ParseResult::email_too_long:
        std::cout << "Email is too long.\n";
        break;
    case serodb::ParseResult::success:
        break;
    }
}

/// Handle a meta‑command and return true if the REPL should exit.
bool handle_meta_command(serodb::MetaCommand cmd, serodb::Table& table)
{
    switch (cmd) {
    case serodb::MetaCommand::Exit:
        table.close();
        return true; // signal exit

    case serodb::MetaCommand::Help:
        std::cout << "Available commands:\n"
                  << "  insert (id, username, email)  — Insert a new row\n"
                  << "  select                        — Display all rows\n"
                  << "  .exit                         — Save and exit\n"
                  << "  .help                         — Show this help\n"
                  << "  .tables                       — List tables\n"
                  << "  .constants                    — Show storage constants\n"
                  << "  .stats                        — Show database statistics\n";
        return false;

    case serodb::MetaCommand::Tables:
        std::cout << "users\n";
        return false;

    case serodb::MetaCommand::Constants:
        std::cout << "Page size:      " << serodb::PAGE_SIZE          << " bytes\n"
                  << "Row size:       " << serodb::SERIALIZED_ROW_SIZE << " bytes\n"
                  << "Rows per page:  " << serodb::ROWS_PER_FULL_PAGE  << "\n"
                  << "Max pages:      " << serodb::MAX_PAGES           << "\n"
                  << "Max rows:       " << serodb::MAX_ROWS            << "\n";
        return false;

    case serodb::MetaCommand::Stats:
        std::cout << "Pages loaded:   " << table.pager().pages_loaded() << "\n"
                  << "Rows:           " << table.row_count()            << "\n"
                  << "Database size:  " << table.pager().file_length()   << " bytes\n";
        return false;
    }
    return false; // unreachable
}

} // anonymous namespace

int main()
{
    const std::string db_path = serodb::DEFAULT_DATABASE_PATH;

    // Open the database table — this creates the Pager and reads the header.
    serodb::Table table(db_path);
    serodb::Parser parser;

    std::string line;
    while (true) {
        print_prompt();
        if (!std::getline(std::cin, line)) {
            break;
        }

        // --- Meta‑commands (start with '.') ---
        if (!line.empty() && line[0] == '.') {
            serodb::MetaCommand cmd{};
            auto mcResult = parser.parse_meta_command(line, cmd);
            if (mcResult == serodb::MetaCommandResult::success) {
                if (handle_meta_command(cmd, table)) {
                    break; // .exit
                }
            } else {
                std::cout << "Unrecognized command: " << line << "\n";
            }
            continue;
        }

        // --- SQL‑like statements ---
        serodb::Statement stmt;
        auto result = parser.prepareStatement(line, stmt);
        if (result == serodb::ParseResult::success) {
            serodb::Executor::execute(stmt, table, std::cout);
        } else {
            print_parse_error(result);
        }
    }

    return 0;
}
