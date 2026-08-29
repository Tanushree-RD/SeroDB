#include "serodb/Parser.hpp"

#include "serodb/Statement.hpp"
#include "serodb/row.hpp"

#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace serodb {
namespace {

std::string trim(const std::string& s)
{
    const auto start = s.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

std::string to_lower_copy(const std::string& s)
{
    std::string result = s;
    for (auto& ch : result) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

} // namespace

bool Parser::parse_id(const std::string& token, std::uint32_t& id) const
{
    if (token.empty()) {
        return false;
    }
    for (const char ch : token) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    // Check for leading zeros on multi-digit numbers.
    if (token.size() > 1 && token[0] == '0') {
        return false;
    }
    try {
        const unsigned long val = std::stoul(token);
        if (val > static_cast<unsigned long>(std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }
        id = static_cast<std::uint32_t>(val);
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

ParseResult Parser::parse(const std::string& input, Statement& outStatement) const
{
    const std::string trimmed = trim(input);
    if (trimmed.empty()) {
        return ParseResult::unrecognized_statement;
    }

    const std::string lower = to_lower_copy(trimmed);

    if (lower.rfind("select", 0) == 0) {
        // Accept "select" or "select *" or "select *  " etc.
        const std::string after = trim(trimmed.substr(6));
        if (after.empty() || after == "*") {
            outStatement.type = StatementType::select;
            return ParseResult::success;
        }
        return ParseResult::unrecognized_statement;
    }

    if (lower.rfind("insert", 0) != 0) {
        return ParseResult::unrecognized_statement;
    }

    // --- Parse INSERT (id, username, email) ---

    // Everything after "insert".
    std::string rest = trim(trimmed.substr(6));

    // Expect opening parenthesis (with optional preceding space already trimmed).
    if (rest.empty() || rest[0] != '(') {
        return ParseResult::syntax_error;
    }
    rest = rest.substr(1); // skip '('

    // Extract id (everything up to the first comma).
    const auto comma1 = rest.find(',');
    if (comma1 == std::string::npos) {
        return ParseResult::syntax_error;
    }
    const std::string id_str = trim(rest.substr(0, comma1));
    rest = rest.substr(comma1 + 1);

    // Extract username (everything up to the next comma).
    const auto comma2 = rest.find(',');
    if (comma2 == std::string::npos) {
        return ParseResult::syntax_error;
    }
    const std::string username_str = trim(rest.substr(0, comma2));
    rest = rest.substr(comma2 + 1);

    // Extract email (everything up to the closing parenthesis).
    const auto paren = rest.find(')');
    if (paren == std::string::npos) {
        return ParseResult::syntax_error;
    }
    const std::string email_str = trim(rest.substr(0, paren));

    // Nothing meaningful should follow the closing parenthesis.
    if (trim(rest.substr(paren + 1)).size() > 1) {
        return ParseResult::syntax_error;
    }

    // Validate id.
    std::uint32_t id{};
    if (!parse_id(id_str, id)) {
        return ParseResult::invalid_id;
    }

    // Validate username length.
    if (username_str.size() > Row::max_username_length) {
        return ParseResult::username_too_long;
    }

    // Validate email length.
    if (email_str.size() > Row::max_email_length) {
        return ParseResult::email_too_long;
    }

    outStatement.type = StatementType::insert;
    outStatement.row.id = id;
    outStatement.row.username = username_str;
    outStatement.row.email = email_str;
    return ParseResult::success;
}

} // namespace serodb
