#include "serodb/Database.hpp"
#include "serodb/Parser.hpp"
#include "serodb/Statement.hpp"
#include "serodb/storage.hpp"

#include <iomanip>
#include <iostream>
#include <string>

namespace {
void print_prompt() {
    std::cout << "serodb > ";
}

void print_parse_error(serodb::ParseResult result) {
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

void print_table(const std::vector<serodb::Row>& rows) {
    constexpr std::size_t min_id_width = 2;
    constexpr std::size_t min_username_width = 8;
    constexpr std::size_t min_email_width = 5;
    std::size_t id_width = min_id_width;
    std::size_t username_width = min_username_width;
    std::size_t email_width = min_email_width;
    for (const auto& row : rows) {
        id_width = std::max(id_width, std::to_string(row.id).size());
        username_width = std::max(username_width, row.username.size());
        email_width = std::max(email_width, row.email.size());
    }
    std::cout << std::left
              << std::setw(static_cast<int>(id_width)) << "id" << " | "
              << std::setw(static_cast<int>(username_width)) << "username" << " | "
              << std::setw(static_cast<int>(email_width)) << "email" << '\n';
    std::cout << std::string(id_width, '-') << "-+-"
              << std::string(username_width, '-') << "-+-"
              << std::string(email_width, '-') << '\n';
    for (const auto& row : rows) {
        std::cout << std::left
                  << std::setw(static_cast<int>(id_width)) << row.id << " | "
                  << std::setw(static_cast<int>(username_width)) << row.username << " | "
                  << std::setw(static_cast<int>(email_width)) << row.email << '\n';
    }
    std::cout << rows.size() << " row(s).\n";
}
} // namespace

int main() {
    const std::string db_path = serodb::default_database_path;
    serodb::Database db;
    try {
        db.load(db_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load database: " << e.what() << '\n';
        return 1;
    }

    serodb::Parser parser;
    std::string line;
    while (true) {
        print_prompt();
        if (!std::getline(std::cin, line)) break;
        if (line == ".exit") break;

        serodb::Statement stmt;
        auto result = parser.parse(line, stmt);
        if (result == serodb::ParseResult::success) {
            try {
                if (stmt.type == serodb::StatementType::insert) {
                    db.insert(stmt.row);
                    db.save(db_path);
                    std::cout << "Executed.\n";
                } else { // select
                    print_table(db.rows());
                }
            } catch (const std::exception& e) {
                std::cerr << "Storage error: " << e.what() << '\n';
            }
        } else {
            print_parse_error(result);
        }
    }
    return 0;
}
