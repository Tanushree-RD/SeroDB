#include "serodb/row.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum class ParseResult {
    success,
    unrecognized_statement,
    syntax_error,
    invalid_id,
    username_too_long,
    email_too_long,
};

enum class StatementType {
    insert,
    select,
};

struct Statement {
    StatementType type{};
    serodb::Row row;
};

void print_prompt()
{
    std::cout << "serodb > ";
}

bool parse_id(const std::string& token, std::uint32_t& id)
{
    if (token.empty()) {
        return false;
    }

    for (const char ch : token) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }

    try {
        const auto value = std::stoull(token);
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }

        id = static_cast<std::uint32_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

ParseResult parse_insert_statement(const std::string& input, Statement& statement)
{
    std::istringstream stream(input);

    std::string statement_token;
    std::string id_token;
    std::string username;
    std::string email;
    std::string extra;

    stream >> statement_token >> id_token >> username >> email;

    if (!stream || (stream >> extra)) {
        return ParseResult::syntax_error;
    }

    std::uint32_t id{};
    if (!parse_id(id_token, id)) {
        return ParseResult::invalid_id;
    }

    if (!serodb::Row::is_valid_username(username)) {
        return ParseResult::username_too_long;
    }

    if (!serodb::Row::is_valid_email(email)) {
        return ParseResult::email_too_long;
    }

    statement.type = StatementType::insert;
    statement.row = serodb::Row{id, username, email};
    return ParseResult::success;
}

ParseResult parse_select_statement(const std::string& input, Statement& statement)
{
    std::istringstream stream(input);

    std::string statement_token;
    std::string extra;

    stream >> statement_token;

    if (stream >> extra) {
        return ParseResult::syntax_error;
    }

    statement.type = StatementType::select;
    return ParseResult::success;
}

ParseResult parse_statement(const std::string& input, Statement& statement)
{
    std::istringstream stream(input);
    std::string statement_token;
    stream >> statement_token;

    if (statement_token == "insert") {
        return parse_insert_statement(input, statement);
    }

    if (statement_token == "select") {
        return parse_select_statement(input, statement);
    }

    return ParseResult::unrecognized_statement;
}

void print_parse_error(ParseResult result)
{
    switch (result) {
    case ParseResult::unrecognized_statement:
        std::cout << "Unrecognized statement.\n";
        break;
    case ParseResult::syntax_error:
        std::cout << "Syntax error.\n";
        break;
    case ParseResult::invalid_id:
        std::cout << "Invalid id. Expected an unsigned 32-bit integer.\n";
        break;
    case ParseResult::username_too_long:
        std::cout << "Username is too long.\n";
        break;
    case ParseResult::email_too_long:
        std::cout << "Email is too long.\n";
        break;
    case ParseResult::success:
        break;
    }
}

void execute_insert_statement(const serodb::Row& row, std::vector<serodb::Row>& rows)
{
    rows.push_back(row);
    std::cout << "Executed.\n";
}

void execute_select_statement(const std::vector<serodb::Row>& rows)
{
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

void execute_statement(const Statement& statement, std::vector<serodb::Row>& rows)
{
    switch (statement.type) {
    case StatementType::insert:
        execute_insert_statement(statement.row, rows);
        break;
    case StatementType::select:
        execute_select_statement(rows);
        break;
    }
}

} // namespace

int main()
{
    std::vector<serodb::Row> rows;
    std::string input;

    while (true) {
        print_prompt();

        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input == ".exit") {
            break;
        }

        Statement statement;
        const auto result = parse_statement(input, statement);

        if (result == ParseResult::success) {
            execute_statement(statement, rows);
        } else {
            print_parse_error(result);
        }
    }

    return 0;
}
