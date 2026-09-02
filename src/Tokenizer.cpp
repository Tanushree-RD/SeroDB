#include "serodb/Tokenizer.hpp"

#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace serodb {

// -----------------------------------------------------------------------
// token_kind_name — human-readable labels for diagnostics
// -----------------------------------------------------------------------

const char* token_kind_name(TokenKind kind)
{
    switch (kind) {
    case TokenKind::Identifier:     return "Identifier";
    case TokenKind::Integer:        return "Integer";
    case TokenKind::String:         return "String";
    case TokenKind::KW_SELECT:      return "SELECT";
    case TokenKind::KW_INSERT:      return "INSERT";
    case TokenKind::KW_INTO:        return "INTO";
    case TokenKind::KW_VALUES:      return "VALUES";
    case TokenKind::KW_FROM:        return "FROM";
    case TokenKind::KW_WHERE:       return "WHERE";
    case TokenKind::KW_CREATE:      return "CREATE";
    case TokenKind::KW_TABLE:       return "TABLE";
    case TokenKind::LeftParen:      return "(";
    case TokenKind::RightParen:     return ")";
    case TokenKind::Comma:          return ",";
    case TokenKind::Semicolon:      return ";";
    case TokenKind::Asterisk:       return "*";
    case TokenKind::Equals:         return "=";
    case TokenKind::NotEquals:      return "<>";
    case TokenKind::Less:           return "<";
    case TokenKind::Greater:        return ">";
    case TokenKind::LessEqual:      return "<=";
    case TokenKind::GreaterEqual:   return ">=";
    case TokenKind::End:            return "End";
    case TokenKind::Error:          return "Error";
    }
    return "Unknown";
}

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

namespace {

std::string to_lower_copy(const std::string& s)
{
    std::string result = s;
    for (auto& ch : result) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

TokenKind keyword_lookup(const std::string& lower)
{
    // clang-format off
    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"select", TokenKind::KW_SELECT},
        {"insert", TokenKind::KW_INSERT},
        {"into",   TokenKind::KW_INTO},
        {"values", TokenKind::KW_VALUES},
        {"from",   TokenKind::KW_FROM},
        {"where",  TokenKind::KW_WHERE},
        {"create", TokenKind::KW_CREATE},
        {"table",  TokenKind::KW_TABLE},
    };
    // clang-format on

    auto it = keywords.find(lower);
    return (it != keywords.end()) ? it->second : TokenKind::Identifier;
}

bool is_identifier_start(char ch)
{
    return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool is_identifier_char(char ch)
{
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Tokenizer — main entry point
// -----------------------------------------------------------------------

std::vector<Token> Tokenizer::tokenize(const std::string& input) const
{
    std::vector<Token> tokens;
    std::size_t pos    = 0;
    std::size_t line   = 1;
    std::size_t column = 1;

    while (pos < input.size()) {
        skip_whitespace_and_comments(input, pos, line, column);
        if (pos >= input.size()) {
            break;
        }

        const char ch = input[pos];

        // --- Single-quoted string -------------------------------------------
        if (ch == '\'') {
            Token tok = scan_string(input, pos, line, column);
            // Advance position by the length of the lexeme.
            const std::size_t len = tok.lexeme.size();
            // Update line/column based on newlines in the lexeme.
            for (std::size_t i = 0; i < len; ++i) {
                if (tok.lexeme[i] == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
            }
            pos += len;
            tokens.push_back(std::move(tok));
            continue;
        }

        // --- Number ---------------------------------------------------------
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            Token tok = scan_integer(input, pos, line, column);
            pos    += tok.lexeme.size();
            column += tok.lexeme.size();
            tokens.push_back(std::move(tok));
            continue;
        }

        // --- Identifier or keyword ------------------------------------------
        if (is_identifier_start(ch)) {
            Token tok = scan_identifier_or_keyword(input, pos, line, column);
            pos    += tok.lexeme.size();
            column += tok.lexeme.size();
            tokens.push_back(std::move(tok));
            continue;
        }

        // --- Punctuation and operators --------------------------------------
        Token punct;
        punct.line   = line;
        punct.column = column;

        switch (ch) {
        case '(':
            punct.kind   = TokenKind::LeftParen;
            punct.lexeme = "(";
            ++pos; ++column;
            break;
        case ')':
            punct.kind   = TokenKind::RightParen;
            punct.lexeme = ")";
            ++pos; ++column;
            break;
        case ',':
            punct.kind   = TokenKind::Comma;
            punct.lexeme = ",";
            ++pos; ++column;
            break;
        case ';':
            punct.kind   = TokenKind::Semicolon;
            punct.lexeme = ";";
            ++pos; ++column;
            break;
        case '*':
            punct.kind   = TokenKind::Asterisk;
            punct.lexeme = "*";
            ++pos; ++column;
            break;
        case '=':
            punct.kind   = TokenKind::Equals;
            punct.lexeme = "=";
            ++pos; ++column;
            break;
        case '<':
            // Could be <, <=, or <>
            if (pos + 1 < input.size()) {
                const char next = input[pos + 1];
                if (next == '=') {
                    punct.kind   = TokenKind::LessEqual;
                    punct.lexeme = "<=";
                    pos += 2; column += 2;
                } else if (next == '>') {
                    punct.kind   = TokenKind::NotEquals;
                    punct.lexeme = "<>";
                    pos += 2; column += 2;
                } else {
                    punct.kind   = TokenKind::Less;
                    punct.lexeme = "<";
                    ++pos; ++column;
                }
            } else {
                punct.kind   = TokenKind::Less;
                punct.lexeme = "<";
                ++pos; ++column;
            }
            break;
        case '>':
            // Could be > or >=
            if (pos + 1 < input.size() && input[pos + 1] == '=') {
                punct.kind   = TokenKind::GreaterEqual;
                punct.lexeme = ">=";
                pos += 2; column += 2;
            } else {
                punct.kind   = TokenKind::Greater;
                punct.lexeme = ">";
                ++pos; ++column;
            }
            break;
        default:
            // Unrecognized character — emit an Error token.
            punct.kind   = TokenKind::Error;
            punct.lexeme = std::string(1, ch);
            ++pos; ++column;
            break;
        }

        tokens.push_back(std::move(punct));
    }

    // Always emit an End token.
    tokens.push_back(Token{TokenKind::End, "", 0, "", line, column});
    return tokens;
}

// -----------------------------------------------------------------------
// Tokenizer — helpers
// -----------------------------------------------------------------------

void Tokenizer::skip_whitespace_and_comments(const std::string& input,
                                              std::size_t& pos,
                                              std::size_t& line,
                                              std::size_t& column) const
{
    while (pos < input.size()) {
        const char ch = input[pos];

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            ++pos;
            ++column;
        } else if (ch == '\n') {
            ++pos;
            ++line;
            column = 1;
        } else if (ch == '-' && pos + 1 < input.size() && input[pos + 1] == '-') {
            // Single-line comment: skip to end of line.
            pos += 2;
            column += 2;
            while (pos < input.size() && input[pos] != '\n') {
                ++pos;
                ++column;
            }
        } else {
            break;
        }
    }
}

Token Tokenizer::scan_identifier_or_keyword(const std::string& input,
                                             std::size_t pos,
                                             std::size_t line,
                                             std::size_t column) const
{
    const std::size_t start = pos;

    while (pos < input.size() && is_identifier_char(input[pos])) {
        ++pos;
    }

    Token token;
    const std::string word = input.substr(start, pos - start);
    token.kind   = keyword_lookup(to_lower_copy(word));
    token.lexeme = word;
    token.line   = line;
    token.column = column;
    return token;
}

Token Tokenizer::scan_integer(const std::string& input,
                               std::size_t pos,
                               std::size_t line,
                               std::size_t column) const
{
    const std::size_t start = pos;

    while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) {
        ++pos;
    }

    Token token;
    token.kind   = TokenKind::Integer;
    token.lexeme = input.substr(start, pos - start);

    try {
        token.int_value = std::stoll(token.lexeme);
    } catch (...) {
        token.kind = TokenKind::Error;
    }

    token.line   = line;
    token.column = column;
    return token;
}

Token Tokenizer::scan_string(const std::string& input,
                              std::size_t pos,
                              std::size_t line,
                              std::size_t column) const
{
    const std::size_t start = pos;
    ++pos; // skip opening '
    std::string value;

    while (pos < input.size()) {
        const char ch = input[pos];
        if (ch == '\'') {
            if (pos + 1 < input.size() && input[pos + 1] == '\'') {
                // Escaped quote: '' → '
                value += '\'';
                pos += 2;
            } else {
                // Closing quote.
                ++pos;
                Token token;
                token.kind      = TokenKind::String;
                token.lexeme    = input.substr(start, pos - start);
                token.str_value = value;
                token.line      = line;
                token.column    = column;
                return token;
            }
        } else {
            if (ch == '\n') {
                ++line;
                column = 0;
            }
            value += ch;
            ++pos;
            ++column;
        }
    }

    // Unterminated string — return what we have plus an implicit end.
    Token token;
    token.kind   = TokenKind::Error;
    token.lexeme = input.substr(start);
    token.line   = line;
    token.column = column;
    return token;
}

} // namespace serodb
