#include "serodb/row.hpp"

#include <cstdint>
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

ParseResult parse_insert_statement(const std::string& input, serodb::Row& row)
{
    std::istringstream stream(input);

    std::string statement;
    std::string id_token;
    std::string username;
    std::string email;
    std::string extra;

    stream >> statement >> id_token >> username >> email;

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

    row = serodb::Row{id, username, email};
    return ParseResult::success;
}

ParseResult parse_statement(const std::string& input, serodb::Row& row)
{
    std::istringstream stream(input);
    std::string statement;
    stream >> statement;

    if (statement == "insert") {
        return parse_insert_statement(input, row);
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
        std::cout << "Syntax error. Expected: insert <id> <username> <email>\n";
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

        serodb::Row row;
        const auto result = parse_statement(input, row);

        if (result == ParseResult::success) {
            rows.push_back(row);
            std::cout << "Executed.\n";
        } else {
            print_parse_error(result);
        }
    }

    return 0;
}
