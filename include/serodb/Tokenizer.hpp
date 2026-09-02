#pragma once

/**
 * Tokenizer.hpp — Lexical analysis (tokenization) for SeroDB SQL input.
 *
 * The Tokenizer converts a raw input string into a sequence of Tokens.
 * It is a standalone lexer with no knowledge of SQL grammar — that job
 * belongs to the Parser, which will consume this token stream later.
 *
 * Design decisions:
 *   • Each Token records its kind, the original lexeme, numeric/string
 *     values when applicable, and source position (line + column) for
 *     error reporting.
 *   • Keywords are recognized as Identifier tokens with a `keyword`
 *     field, keeping the enum small while letting the parser do keyword
 *     lookups.
 *   • The tokenizer is stateless — you construct one, call `tokenize()`,
 *     and get back a `std::vector<Token>`.
 *   • Single-quoted strings use SQL-style escape: '' → '.
 *   • Single-line comments start with -- and run to end of line.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace serodb {

// -----------------------------------------------------------------------
// Token types
// -----------------------------------------------------------------------

/// The kind of token produced by the lexer.
enum class TokenKind {
    // Literals
    Identifier,  // column names, table names, unquoted words
    Integer,     // unsigned integer literal: 0, 1, 42, …
    String,      // single-quoted string: 'hello', 'it''s'

    // Keywords (recognized by spelling, returned as Identifier + keyword)
    KW_SELECT,
    KW_INSERT,
    KW_INTO,
    KW_VALUES,
    KW_FROM,
    KW_WHERE,
    KW_CREATE,
    KW_TABLE,

    // Punctuation / operators
    LeftParen,   // (
    RightParen,  // )
    Comma,       // ,
    Semicolon,   // ;
    Asterisk,    // *
    Equals,      // =
    NotEquals,   // <>
    Less,        // <
    Greater,     // >
    LessEqual,   // <=
    GreaterEqual,// >=

    // Special
    End,         // end of input
    Error,       // unrecognized character / unterminated string
};

/// Return a human-readable name for a TokenKind (useful for diagnostics).
const char* token_kind_name(TokenKind kind);

// -----------------------------------------------------------------------
// Token
// -----------------------------------------------------------------------

struct Token {
    TokenKind kind{};
    std::string lexeme;   // the raw text that was scanned
    std::int64_t int_value = 0;  // populated for Integer tokens
    std::string str_value;        // populated for String tokens (unescaped)

    // Source position (1-based) for error messages.
    std::size_t line   = 1;
    std::size_t column = 1;
};

// -----------------------------------------------------------------------
// Tokenizer
// -----------------------------------------------------------------------

class Tokenizer {
public:
    /// Tokenize the entire input string and return the token stream.
    /// Always ends with a TokenKind::End token (or TokenKind::Error
    /// if something went wrong mid-scan).
    std::vector<Token> tokenize(const std::string& input) const;

private:
    // Internal scanning helpers — all operate on the input and an
    // advancing position index.
    void skip_whitespace_and_comments(const std::string& input,
                                      std::size_t& pos,
                                      std::size_t& line,
                                      std::size_t& column) const;

    Token scan_identifier_or_keyword(const std::string& input,
                                     std::size_t pos,
                                     std::size_t line,
                                     std::size_t column) const;

    Token scan_integer(const std::string& input,
                       std::size_t pos,
                       std::size_t line,
                       std::size_t column) const;

    Token scan_string(const std::string& input,
                      std::size_t pos,
                      std::size_t line,
                      std::size_t column) const;
};

} // namespace serodb
