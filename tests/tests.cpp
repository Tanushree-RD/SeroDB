/**
 * tests.cpp — Self-contained test suite for SeroDB.
 *
 * Uses a simple assertion macro and test-runner pattern — no external
 * framework required.  Each test function runs independently and cleans
 * up its temporary database file on completion.
 *
 * Build and run:
 *   cmake -S . -B build && cmake --build build && ctest --test-dir build
 */

#include "serodb/Constants.hpp"
#include "serodb/Cursor.hpp"
#include "serodb/Executor.hpp"
#include "serodb/Pager.hpp"
#include "serodb/Parser.hpp"
#include "serodb/Statement.hpp"
#include "serodb/Table.hpp"
#include "serodb/Tokenizer.hpp"
#include "serodb/row.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// -----------------------------------------------------------------------
// Minimal test framework
// -----------------------------------------------------------------------

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ASSERT_TRUE(expr)                                                     \
    do {                                                                      \
        if (!(expr)) {                                                        \
            std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__            \
                      << ": " #expr << "\n";                                  \
            return false;                                                     \
        }                                                                     \
    } while (false)

#define ASSERT_EQ(a, b)                                                       \
    do {                                                                      \
        if ((a) != (b)) {                                                     \
            std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__            \
                      << ": " #a " == " #b "\n";                              \
            return false;                                                     \
        }                                                                     \
    } while (false)

#define ASSERT_NE(a, b)                                                       \
    do {                                                                      \
        if ((a) == (b)) {                                                     \
            std::cerr << "  FAIL: " << __FILE__ << ":" << __LINE__            \
                      << ": " #a " != " #b "\n";                             \
            return false;                                                     \
        }                                                                     \
    } while (false)

/// RAII helper that deletes a file on scope exit.
struct TempFile {
    std::string path;
    explicit TempFile(const std::string& p) : path(p) {}
    ~TempFile() { std::remove(path.c_str()); }
};

static void run_test(const std::string& name, std::function<bool()> fn)
{
    ++g_tests_run;
    std::cout << "[ RUN  ] " << name << "\n";
    try {
        if (fn()) {
            ++g_tests_passed;
            std::cout << "[ PASS ] " << name << "\n";
        } else {
            ++g_tests_failed;
            std::cout << "[ FAIL ] " << name << "\n";
        }
    } catch (const std::exception& e) {
        ++g_tests_failed;
        std::cerr << "  EXCEPTION: " << e.what() << "\n";
        std::cout << "[ FAIL ] " << name << "\n";
    }
}

// -----------------------------------------------------------------------
// Test: Row validation
// -----------------------------------------------------------------------

static bool test_row_validation()
{
    serodb::Row valid;
    valid.id = 1;
    valid.username = "alice";
    valid.email = "alice@example.com";
    ASSERT_TRUE(valid.is_valid());

    // Username too long.
    serodb::Row bad_username;
    bad_username.id = 2;
    bad_username.username = std::string(serodb::Row::max_username_length + 1, 'x');
    bad_username.email = "x@x.com";
    ASSERT_TRUE(!bad_username.is_valid());

    // Email too long.
    serodb::Row bad_email;
    bad_email.id = 3;
    bad_email.username = "bob";
    bad_email.email = std::string(serodb::Row::max_email_length + 1, 'e');
    ASSERT_TRUE(!bad_email.is_valid());

    // Exactly at the limit — should be valid.
    serodb::Row edge;
    edge.id = 4;
    edge.username = std::string(serodb::Row::max_username_length, 'u');
    edge.email = std::string(serodb::Row::max_email_length, 'e');
    ASSERT_TRUE(edge.is_valid());

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — valid INSERT
// -----------------------------------------------------------------------

static bool test_parser_insert()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    auto r = parser.prepareStatement("insert (1, alice, alice@example.com)", stmt);
    ASSERT_EQ(r, serodb::ParseResult::success);
    ASSERT_EQ(stmt.type, serodb::StatementType::Insert);
    ASSERT_EQ(stmt.row.id, 1u);
    ASSERT_EQ(stmt.row.username, "alice");
    ASSERT_EQ(stmt.row.email, "alice@example.com");

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — valid SELECT variants
// -----------------------------------------------------------------------

static bool test_parser_select()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    ASSERT_EQ(parser.prepareStatement("select", stmt), serodb::ParseResult::success);
    ASSERT_EQ(stmt.type, serodb::StatementType::Select);

    ASSERT_EQ(parser.prepareStatement("select *", stmt), serodb::ParseResult::success);
    ASSERT_EQ(parser.prepareStatement("SELECT", stmt), serodb::ParseResult::success);
    ASSERT_EQ(parser.prepareStatement("  select  *  ", stmt), serodb::ParseResult::success);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — invalid commands
// -----------------------------------------------------------------------

static bool test_parser_invalid()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    ASSERT_EQ(parser.prepareStatement("", stmt), serodb::ParseResult::unrecognized_statement);
    ASSERT_EQ(parser.prepareStatement("delete", stmt), serodb::ParseResult::unrecognized_statement);
    ASSERT_EQ(parser.prepareStatement("insert", stmt), serodb::ParseResult::syntax_error);
    ASSERT_EQ(parser.prepareStatement("insert (abc, a, a@b.com)", stmt), serodb::ParseResult::invalid_id);

    // Username too long.
    std::string long_user = "insert (1, " + std::string(33, 'u') + ", a@b.com)";
    ASSERT_EQ(parser.prepareStatement(long_user, stmt), serodb::ParseResult::username_too_long);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — meta-commands
// -----------------------------------------------------------------------

static bool test_parser_meta_commands()
{
    serodb::Parser parser;
    serodb::MetaCommand cmd{};

    ASSERT_EQ(parser.parse_meta_command(".exit", cmd), serodb::MetaCommandResult::success);
    ASSERT_EQ(cmd, serodb::MetaCommand::Exit);

    ASSERT_EQ(parser.parse_meta_command(".help", cmd), serodb::MetaCommandResult::success);
    ASSERT_EQ(cmd, serodb::MetaCommand::Help);

    ASSERT_EQ(parser.parse_meta_command(".tables", cmd), serodb::MetaCommandResult::success);
    ASSERT_EQ(cmd, serodb::MetaCommand::Tables);

    ASSERT_EQ(parser.parse_meta_command(".constants", cmd), serodb::MetaCommandResult::success);
    ASSERT_EQ(cmd, serodb::MetaCommand::Constants);

    ASSERT_EQ(parser.parse_meta_command(".stats", cmd), serodb::MetaCommandResult::success);
    ASSERT_EQ(cmd, serodb::MetaCommand::Stats);

    ASSERT_EQ(parser.parse_meta_command(".unknown", cmd), serodb::MetaCommandResult::unrecognized_command);

    return true;
}

// -----------------------------------------------------------------------
// Test: Pager — create, write, close, reopen, read
// -----------------------------------------------------------------------

static bool test_pager_round_trip()
{
    const std::string path = "test_pager_rt.db";
    TempFile tmp(path);

    // Write some data to page 0.
    {
        serodb::Pager pager(path);
        char* page = pager.get_page(0);
        std::memcpy(page, "HELLO_DB", 8);
        pager.flush(0);
        pager.close();
    }

    // Reopen and verify.
    {
        serodb::Pager pager(path);
        char* page = pager.get_page(0);
        ASSERT_TRUE(std::memcmp(page, "HELLO_DB", 8) == 0);
        pager.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Pager — pages_loaded count
// -----------------------------------------------------------------------

static bool test_pager_stats()
{
    const std::string path = "test_pager_stats.db";
    TempFile tmp(path);

    serodb::Pager pager(path);
    ASSERT_EQ(pager.pages_loaded(), static_cast<std::size_t>(0));

    pager.get_page(0);
    ASSERT_EQ(pager.pages_loaded(), static_cast<std::size_t>(1));

    pager.get_page(3);
    ASSERT_EQ(pager.pages_loaded(), static_cast<std::size_t>(2));

    // Requesting the same page again should not increase the count.
    pager.get_page(0);
    ASSERT_EQ(pager.pages_loaded(), static_cast<std::size_t>(2));

    pager.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Pager — flush writes to disk
// -----------------------------------------------------------------------

static bool test_pager_flush()
{
    const std::string path = "test_pager_flush.db";
    TempFile tmp(path);

    {
        serodb::Pager pager(path);
        char* page = pager.get_page(0);
        std::memset(page, 'A', serodb::PAGE_SIZE);
        pager.flush(0);
        pager.close();
    }

    // Verify the file is at least PAGE_SIZE bytes.
    std::ifstream check(path, std::ios::binary | std::ios::ate);
    ASSERT_TRUE(check.is_open());
    auto size = check.tellg();
    ASSERT_TRUE(static_cast<std::size_t>(size) >= serodb::PAGE_SIZE);

    return true;
}

// -----------------------------------------------------------------------
// Test: Serialization round-trip (via Table internals)
// -----------------------------------------------------------------------

static bool test_serialization_round_trip()
{
    const std::string path = "test_serial_rt.db";
    TempFile tmp(path);

    serodb::Row original;
    original.id = 42;
    original.username = "testuser";
    original.email = "test@example.com";

    {
        serodb::Table table(path);
        table.insert(original);
        table.close();
    }

    {
        serodb::Table table(path);
        ASSERT_EQ(table.row_count(), static_cast<std::size_t>(1));

        auto cursor = serodb::Cursor::table_start(table);
        ASSERT_TRUE(!cursor.end());

        serodb::Row loaded = cursor.read();
        ASSERT_EQ(loaded.id, original.id);
        ASSERT_EQ(loaded.username, original.username);
        ASSERT_EQ(loaded.email, original.email);

        table.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Table — insert multiple rows
// -----------------------------------------------------------------------

static bool test_table_insert_multiple()
{
    const std::string path = "test_multi_insert.db";
    TempFile tmp(path);

    constexpr std::size_t N = 20;
    {
        serodb::Table table(path);
        for (std::size_t i = 0; i < N; ++i) {
            serodb::Row row;
            row.id = static_cast<std::uint32_t>(i + 1);
            row.username = "user" + std::to_string(i);
            row.email = "user" + std::to_string(i) + "@test.com";
            table.insert(row);
        }
        ASSERT_EQ(table.row_count(), N);
        table.close();
    }

    // Reopen and verify all rows.
    {
        serodb::Table table(path);
        ASSERT_EQ(table.row_count(), N);

        std::size_t count = 0;
        for (auto c = serodb::Cursor::table_start(table); !c.end(); c.advance()) {
            serodb::Row row = c.read();
            ASSERT_EQ(row.id, static_cast<std::uint32_t>(count + 1));
            ASSERT_EQ(row.username, "user" + std::to_string(count));
            ++count;
        }
        ASSERT_EQ(count, N);

        table.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Table — persistence across restart
// -----------------------------------------------------------------------

static bool test_table_persistence()
{
    const std::string path = "test_persist.db";
    TempFile tmp(path);

    // Insert 5 rows.
    {
        serodb::Table table(path);
        for (int i = 1; i <= 5; ++i) {
            serodb::Row row;
            row.id = static_cast<std::uint32_t>(i);
            row.username = "user" + std::to_string(i);
            row.email = "u" + std::to_string(i) + "@db.com";
            table.insert(row);
        }
        table.close();
    }

    // Reopen, insert 3 more, close.
    {
        serodb::Table table(path);
        ASSERT_EQ(table.row_count(), static_cast<std::size_t>(5));

        for (int i = 6; i <= 8; ++i) {
            serodb::Row row;
            row.id = static_cast<std::uint32_t>(i);
            row.username = "user" + std::to_string(i);
            row.email = "u" + std::to_string(i) + "@db.com";
            table.insert(row);
        }
        table.close();
    }

    // Final reopen — should see all 8 rows.
    {
        serodb::Table table(path);
        ASSERT_EQ(table.row_count(), static_cast<std::size_t>(8));

        auto c = serodb::Cursor::table_start(table);
        for (std::uint32_t i = 1; i <= 8; ++i) {
            ASSERT_TRUE(!c.end());
            serodb::Row row = c.read();
            ASSERT_EQ(row.id, i);
            c.advance();
        }
        ASSERT_TRUE(c.end());

        table.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Empty database
// -----------------------------------------------------------------------

static bool test_empty_database()
{
    const std::string path = "test_empty.db";
    TempFile tmp(path);

    serodb::Table table(path);
    ASSERT_EQ(table.row_count(), static_cast<std::size_t>(0));

    auto cursor = serodb::Cursor::table_start(table);
    ASSERT_TRUE(cursor.end());

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Cursor on empty table
// -----------------------------------------------------------------------

static bool test_cursor_empty()
{
    const std::string path = "test_cursor_empty.db";
    TempFile tmp(path);

    serodb::Table table(path);

    auto start = serodb::Cursor::table_start(table);
    auto end   = serodb::Cursor::table_end(table);

    ASSERT_TRUE(start.end());
    ASSERT_TRUE(end.end());
    ASSERT_EQ(start.row_num(), static_cast<std::size_t>(0));
    ASSERT_EQ(end.row_num(), static_cast<std::size_t>(0));

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Cursor traversal
// -----------------------------------------------------------------------

static bool test_cursor_traversal()
{
    const std::string path = "test_cursor_trav.db";
    TempFile tmp(path);

    constexpr std::size_t N = 10;
    {
        serodb::Table table(path);
        for (std::size_t i = 0; i < N; ++i) {
            serodb::Row row;
            row.id = static_cast<std::uint32_t>(i);
            row.username = "u" + std::to_string(i);
            row.email = "u" + std::to_string(i) + "@t.com";
            table.insert(row);
        }
        table.close();
    }

    {
        serodb::Table table(path);
        std::size_t count = 0;
        for (auto c = serodb::Cursor::table_start(table); !c.end(); c.advance()) {
            ASSERT_EQ(c.row_num(), count);
            serodb::Row row = c.read();
            ASSERT_EQ(row.id, static_cast<std::uint32_t>(count));
            ++count;
        }
        ASSERT_EQ(count, N);
        table.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Multiple page spanning
// -----------------------------------------------------------------------

static bool test_multi_page()
{
    const std::string path = "test_multi_page.db";
    TempFile tmp(path);

    // Insert enough rows to span at least 3 pages.
    // Page 0: ROWS_IN_PAGE_0 rows = 14, Page 1: 14 rows, Page 2: partial
    const std::size_t N = serodb::ROWS_IN_PAGE_0 + serodb::ROWS_PER_FULL_PAGE + 5;

    {
        serodb::Table table(path);
        for (std::size_t i = 0; i < N; ++i) {
            serodb::Row row;
            row.id = static_cast<std::uint32_t>(i + 1);
            row.username = "mp" + std::to_string(i);
            row.email = "mp" + std::to_string(i) + "@page.com";
            table.insert(row);
        }
        ASSERT_EQ(table.row_count(), N);
        table.close();
    }

    // Reopen and verify all rows survived across page boundaries.
    {
        serodb::Table table(path);
        ASSERT_EQ(table.row_count(), N);

        std::size_t count = 0;
        for (auto c = serodb::Cursor::table_start(table); !c.end(); c.advance()) {
            serodb::Row row = c.read();
            ASSERT_EQ(row.id, static_cast<std::uint32_t>(count + 1));
            ASSERT_EQ(row.username, "mp" + std::to_string(count));
            ++count;
        }
        ASSERT_EQ(count, N);

        table.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Executor — INSERT + SELECT via statement execution
// -----------------------------------------------------------------------

static bool test_executor()
{
    const std::string path = "test_executor.db";
    TempFile tmp(path);

    serodb::Table table(path);
    std::ostringstream out;

    // Execute an INSERT.
    serodb::Statement insert_stmt;
    insert_stmt.type = serodb::StatementType::Insert;
    insert_stmt.row.id = 99;
    insert_stmt.row.username = "executor";
    insert_stmt.row.email = "exec@test.com";

    auto r = serodb::Executor::execute(insert_stmt, table, out);
    ASSERT_EQ(r, serodb::ExecuteResult::success);
    ASSERT_TRUE(out.str().find("Executed") != std::string::npos);

    // Execute a SELECT.
    out.str("");
    serodb::Statement select_stmt;
    select_stmt.type = serodb::StatementType::Select;

    r = serodb::Executor::execute(select_stmt, table, out);
    ASSERT_EQ(r, serodb::ExecuteResult::success);
    ASSERT_TRUE(out.str().find("executor") != std::string::npos);
    ASSERT_TRUE(out.str().find("1 row(s)") != std::string::npos);

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Constants sanity check
// -----------------------------------------------------------------------

static bool test_constants()
{
    // Verify the row size calculation matches the field sizes.
    ASSERT_EQ(serodb::SERIALIZED_ROW_SIZE,
              sizeof(std::uint32_t) + serodb::Row::max_username_length + serodb::Row::max_email_length);

    // Verify header size.
    ASSERT_EQ(serodb::HEADER_SIZE, static_cast<std::size_t>(12));

    // Verify rows fit in page 0 after the header.
    ASSERT_TRUE(serodb::ROWS_IN_PAGE_0 * serodb::SERIALIZED_ROW_SIZE + serodb::HEADER_SIZE
                <= serodb::PAGE_SIZE);

    // Verify rows per full page.
    ASSERT_TRUE(serodb::ROWS_PER_FULL_PAGE * serodb::SERIALIZED_ROW_SIZE
                <= serodb::PAGE_SIZE);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — email too long
// -----------------------------------------------------------------------

static bool test_parser_email_too_long()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    // Email exactly at the limit should succeed.
    std::string at_limit = "insert (1, alice, " + std::string(serodb::Row::max_email_length, 'e') + ")";
    ASSERT_EQ(parser.prepareStatement(at_limit, stmt), serodb::ParseResult::success);

    // Email one byte over the limit.
    std::string over = "insert (1, alice, " + std::string(serodb::Row::max_email_length + 1, 'e') + ")";
    ASSERT_EQ(parser.prepareStatement(over, stmt), serodb::ParseResult::email_too_long);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — extra text after valid statement
// -----------------------------------------------------------------------

static bool test_parser_extra_trailing_text()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    // Extra text after a valid INSERT.
    ASSERT_EQ(parser.prepareStatement("insert (1, a, b) junk", stmt),
              serodb::ParseResult::syntax_error);

    // Extra text after a valid SELECT.
    ASSERT_EQ(parser.prepareStatement("select * extra", stmt),
              serodb::ParseResult::unrecognized_statement);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — missing closing parenthesis
// -----------------------------------------------------------------------

static bool test_parser_missing_close_paren()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    ASSERT_EQ(parser.prepareStatement("insert (1, a, b", stmt),
              serodb::ParseResult::syntax_error);

    // Missing both parens.
    ASSERT_EQ(parser.prepareStatement("insert 1, a, b", stmt),
              serodb::ParseResult::syntax_error);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — id boundary values
// -----------------------------------------------------------------------

static bool test_parser_id_boundary()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    // id = 0 is valid.
    ASSERT_EQ(parser.prepareStatement("insert (0, alice, a@b.com)", stmt),
              serodb::ParseResult::success);
    ASSERT_EQ(stmt.row.id, 0u);

    // id = UINT32_MAX is valid.
    ASSERT_EQ(parser.prepareStatement("insert (4294967295, alice, a@b.com)", stmt),
              serodb::ParseResult::success);
    ASSERT_EQ(stmt.row.id, 4294967295u);

    // id = UINT32_MAX + 1 is invalid.
    ASSERT_EQ(parser.prepareStatement("insert (4294967296, alice, a@b.com)", stmt),
              serodb::ParseResult::invalid_id);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — id with leading zeros
// -----------------------------------------------------------------------

static bool test_parser_id_leading_zeros()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    // Single zero is fine.
    ASSERT_EQ(parser.prepareStatement("insert (0, a, b)", stmt),
              serodb::ParseResult::success);

    // Leading zeros on multi-digit numbers are rejected.
    ASSERT_EQ(parser.prepareStatement("insert (01, a, b)", stmt),
              serodb::ParseResult::invalid_id);

    ASSERT_EQ(parser.prepareStatement("insert (007, a, b)", stmt),
              serodb::ParseResult::invalid_id);

    return true;
}

// -----------------------------------------------------------------------
// Test: Parser — SELECT is case-insensitive
// -----------------------------------------------------------------------

static bool test_parser_select_case_insensitive()
{
    serodb::Parser parser;
    serodb::Statement stmt;

    ASSERT_EQ(parser.prepareStatement("SELECT", stmt), serodb::ParseResult::success);
    ASSERT_EQ(stmt.type, serodb::StatementType::Select);

    ASSERT_EQ(parser.prepareStatement("Select", stmt), serodb::ParseResult::success);
    ASSERT_EQ(stmt.type, serodb::StatementType::Select);

    ASSERT_EQ(parser.prepareStatement("  SELECT  *  ", stmt), serodb::ParseResult::success);

    return true;
}

// -----------------------------------------------------------------------
// Test: Row — empty fields are valid
// -----------------------------------------------------------------------

static bool test_row_empty_fields()
{
    serodb::Row row;
    row.id = 1;
    row.username = "";
    row.email = "";
    ASSERT_TRUE(row.is_valid());
    ASSERT_TRUE(serodb::Row::is_valid_username(""));
    ASSERT_TRUE(serodb::Row::is_valid_email(""));

    return true;
}

// -----------------------------------------------------------------------
// Test: Row — id boundary values
// -----------------------------------------------------------------------

static bool test_row_id_boundary()
{
    serodb::Row row;
    row.username = "alice";
    row.email = "a@b.com";

    // id = 0
    row.id = 0;
    ASSERT_TRUE(row.is_valid());

    // id = UINT32_MAX
    row.id = std::numeric_limits<std::uint32_t>::max();
    ASSERT_TRUE(row.is_valid());

    return true;
}

// -----------------------------------------------------------------------
// Test: Table — insert invalid row throws
// -----------------------------------------------------------------------

static bool test_table_insert_invalid_row()
{
    const std::string path = "test_insert_invalid.db";
    TempFile tmp(path);

    serodb::Table table(path);

    // Username too long.
    serodb::Row bad_user;
    bad_user.id = 1;
    bad_user.username = std::string(serodb::Row::max_username_length + 1, 'x');
    bad_user.email = "a@b.com";

    bool threw = false;
    try {
        table.insert(bad_user);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
    ASSERT_EQ(table.row_count(), static_cast<std::size_t>(0));

    // Email too long.
    serodb::Row bad_email;
    bad_email.id = 2;
    bad_email.username = "alice";
    bad_email.email = std::string(serodb::Row::max_email_length + 1, 'e');

    threw = false;
    try {
        table.insert(bad_email);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
    ASSERT_EQ(table.row_count(), static_cast<std::size_t>(0));

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Table — insert when full throws
// -----------------------------------------------------------------------

static bool test_table_insert_full()
{
    const std::string path = "test_table_full.db";
    TempFile tmp(path);

    serodb::Table table(path);

    // Fill to MAX_ROWS.
    for (std::size_t i = 0; i < serodb::MAX_ROWS; ++i) {
        serodb::Row row;
        row.id = static_cast<std::uint32_t>(i);
        row.username = "u" + std::to_string(i);
        row.email = "u" + std::to_string(i) + "@t.com";
        table.insert(row);
    }
    ASSERT_EQ(table.row_count(), serodb::MAX_ROWS);

    // One more should fail.
    serodb::Row extra;
    extra.id = 999999;
    extra.username = "overflow";
    extra.email = "o@t.com";

    bool threw = false;
    try {
        table.insert(extra);
    } catch (const std::runtime_error& e) {
        threw = true;
        // Verify the error message mentions "full".
        std::string msg = e.what();
        ASSERT_TRUE(msg.find("full") != std::string::npos);
    }
    ASSERT_TRUE(threw);

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Executor — SELECT on empty table
// -----------------------------------------------------------------------

static bool test_executor_select_empty()
{
    const std::string path = "test_exec_empty.db";
    TempFile tmp(path);

    serodb::Table table(path);
    std::ostringstream out;

    serodb::Statement stmt;
    stmt.type = serodb::StatementType::Select;

    auto r = serodb::Executor::execute(stmt, table, out);
    ASSERT_EQ(r, serodb::ExecuteResult::success);
    ASSERT_TRUE(out.str().find("0 row(s)") != std::string::npos);

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Executor — multiple INSERTs then SELECT
// -----------------------------------------------------------------------

static bool test_executor_multiple_inserts()
{
    const std::string path = "test_exec_multi.db";
    TempFile tmp(path);

    serodb::Table table(path);
    std::ostringstream out;

    for (int i = 1; i <= 5; ++i) {
        serodb::Statement stmt;
        stmt.type = serodb::StatementType::Insert;
        stmt.row.id = static_cast<std::uint32_t>(i);
        stmt.row.username = "user" + std::to_string(i);
        stmt.row.email = "u" + std::to_string(i) + "@test.com";

        auto r = serodb::Executor::execute(stmt, table, out);
        ASSERT_EQ(r, serodb::ExecuteResult::success);
    }

    ASSERT_EQ(table.row_count(), static_cast<std::size_t>(5));

    // SELECT should show all 5 rows.
    out.str("");
    serodb::Statement sel;
    sel.type = serodb::StatementType::Select;
    auto r = serodb::Executor::execute(sel, table, out);
    ASSERT_EQ(r, serodb::ExecuteResult::success);
    ASSERT_TRUE(out.str().find("5 row(s)") != std::string::npos);
    ASSERT_TRUE(out.str().find("user1") != std::string::npos);
    ASSERT_TRUE(out.str().find("user5") != std::string::npos);

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Executor — invalid row produces error output
// -----------------------------------------------------------------------

static bool test_executor_invalid_insert()
{
    const std::string path = "test_exec_invalid.db";
    TempFile tmp(path);

    serodb::Table table(path);
    std::ostringstream out;

    serodb::Statement stmt;
    stmt.type = serodb::StatementType::Insert;
    stmt.row.id = 1;
    stmt.row.username = std::string(serodb::Row::max_username_length + 1, 'x');
    stmt.row.email = "a@b.com";

    auto r = serodb::Executor::execute(stmt, table, out);
    ASSERT_EQ(r, serodb::ExecuteResult::error);
    ASSERT_TRUE(out.str().find("Error") != std::string::npos);
    ASSERT_EQ(table.row_count(), static_cast<std::size_t>(0));

    table.close();
    return true;
}

// -----------------------------------------------------------------------
// Test: Serialization format — raw byte layout verification
// -----------------------------------------------------------------------

static bool test_serialization_format()
{
    const std::string path = "test_serial_fmt.db";
    TempFile tmp(path);

    serodb::Row row;
    row.id = 42;
    row.username = "alice";
    row.email = "a@b.com";

    {
        serodb::Table table(path);
        table.insert(row);
        table.close();
    }

    // Read raw page 0 and verify the on-disk layout.
    {
        serodb::Pager pager(path);
        char* page0 = pager.get_page(0);

        // Bytes 0..7: magic "SeroDB1\0".
        ASSERT_TRUE(std::memcmp(page0, "SeroDB1\0", 8) == 0);

        // Bytes 8..11: row count = 1 (little-endian uint32).
        ASSERT_EQ(static_cast<unsigned char>(page0[8]), 1u);
        ASSERT_EQ(static_cast<unsigned char>(page0[9]), 0u);
        ASSERT_EQ(static_cast<unsigned char>(page0[10]), 0u);
        ASSERT_EQ(static_cast<unsigned char>(page0[11]), 0u);

        // Row data starts at offset HEADER_SIZE (12).
        const char* row_data = page0 + serodb::HEADER_SIZE;

        // First 4 bytes: id = 42 in little-endian.
        ASSERT_EQ(static_cast<unsigned char>(row_data[0]), 42u);
        ASSERT_EQ(static_cast<unsigned char>(row_data[1]), 0u);
        ASSERT_EQ(static_cast<unsigned char>(row_data[2]), 0u);
        ASSERT_EQ(static_cast<unsigned char>(row_data[3]), 0u);

        // Next 32 bytes: username "alice" + null padding.
        ASSERT_TRUE(std::memcmp(row_data + 4, "alice", 5) == 0);
        ASSERT_EQ(static_cast<unsigned char>(row_data[4 + 5]), 0u); // null padding starts

        // Next 255 bytes: email "a@b.com" + null padding.
        ASSERT_TRUE(std::memcmp(row_data + 4 + 32, "a@b.com", 7) == 0);
        ASSERT_EQ(static_cast<unsigned char>(row_data[4 + 32 + 7]), 0u); // null padding starts

        pager.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Table — row_slot boundary between page 0 and page 1
// -----------------------------------------------------------------------

static bool test_table_row_slot_boundary()
{
    const std::string path = "test_slot_bound.db";
    TempFile tmp(path);

    // Insert exactly ROWS_IN_PAGE_0 rows — last one is the final row on page 0.
    {
        serodb::Table table(path);
        for (std::size_t i = 0; i < serodb::ROWS_IN_PAGE_0; ++i) {
            serodb::Row row;
            row.id = static_cast<std::uint32_t>(i);
            row.username = "p0" + std::to_string(i);
            row.email = "p0" + std::to_string(i) + "@t.com";
            table.insert(row);
        }
        ASSERT_EQ(table.row_count(), serodb::ROWS_IN_PAGE_0);
        table.close();
    }

    // Insert one more — should land on page 1.
    {
        serodb::Table table(path);
        serodb::Row row;
        row.id = 999;
        row.username = "page1user";
        row.email = "p1@t.com";
        table.insert(row);
        ASSERT_EQ(table.row_count(), serodb::ROWS_IN_PAGE_0 + 1);
        table.close();
    }

    // Reopen and verify all rows including the cross-page one.
    {
        serodb::Table table(path);
        ASSERT_EQ(table.row_count(), serodb::ROWS_IN_PAGE_0 + 1);

        std::size_t count = 0;
        for (auto c = serodb::Cursor::table_start(table); !c.end(); c.advance()) {
            serodb::Row row = c.read();
            if (count < serodb::ROWS_IN_PAGE_0) {
                ASSERT_EQ(row.id, static_cast<std::uint32_t>(count));
            } else {
                ASSERT_EQ(row.id, 999u);
                ASSERT_EQ(row.username, "page1user");
            }
            ++count;
        }
        ASSERT_EQ(count, serodb::ROWS_IN_PAGE_0 + 1);

        table.close();
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — keywords
// -----------------------------------------------------------------------

static bool test_tokenizer_keywords()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("SELECT INSERT INTO VALUES FROM WHERE CREATE TABLE");
    // 8 keywords + End = 9 tokens
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(9));
    ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_SELECT);
    ASSERT_EQ(tokens[1].kind, serodb::TokenKind::KW_INSERT);
    ASSERT_EQ(tokens[2].kind, serodb::TokenKind::KW_INTO);
    ASSERT_EQ(tokens[3].kind, serodb::TokenKind::KW_VALUES);
    ASSERT_EQ(tokens[4].kind, serodb::TokenKind::KW_FROM);
    ASSERT_EQ(tokens[5].kind, serodb::TokenKind::KW_WHERE);
    ASSERT_EQ(tokens[6].kind, serodb::TokenKind::KW_CREATE);
    ASSERT_EQ(tokens[7].kind, serodb::TokenKind::KW_TABLE);
    ASSERT_EQ(tokens[8].kind, serodb::TokenKind::End);

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — keywords are case-insensitive
// -----------------------------------------------------------------------

static bool test_tokenizer_keyword_case()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("select Select SELECT sElEcT");
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(5)); // 4 + End
    for (std::size_t i = 0; i < 4; ++i) {
        ASSERT_EQ(tokens[i].kind, serodb::TokenKind::KW_SELECT);
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — identifiers
// -----------------------------------------------------------------------

static bool test_tokenizer_identifiers()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("users id user_name _private");
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(5)); // 4 + End
    for (std::size_t i = 0; i < 4; ++i) {
        ASSERT_EQ(tokens[i].kind, serodb::TokenKind::Identifier);
    }
    ASSERT_EQ(tokens[0].lexeme, "users");
    ASSERT_EQ(tokens[1].lexeme, "id");
    ASSERT_EQ(tokens[2].lexeme, "user_name");
    ASSERT_EQ(tokens[3].lexeme, "_private");

    // Identifier with digits.
    auto tokens2 = tok.tokenize("col1 table2");
    ASSERT_EQ(tokens2.size(), static_cast<std::size_t>(3));
    ASSERT_EQ(tokens2[0].kind, serodb::TokenKind::Identifier);
    ASSERT_EQ(tokens2[0].lexeme, "col1");

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — integers
// -----------------------------------------------------------------------

static bool test_tokenizer_integers()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("0 1 42 999 1234567890");
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(6)); // 5 + End
    for (std::size_t i = 0; i < 5; ++i) {
        ASSERT_EQ(tokens[i].kind, serodb::TokenKind::Integer);
    }
    ASSERT_EQ(tokens[0].int_value, 0);
    ASSERT_EQ(tokens[1].int_value, 1);
    ASSERT_EQ(tokens[2].int_value, 42);
    ASSERT_EQ(tokens[3].int_value, 999);
    ASSERT_EQ(tokens[4].int_value, 1234567890);

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — single-quoted strings
// -----------------------------------------------------------------------

static bool test_tokenizer_strings()
{
    serodb::Tokenizer tok;

    // Simple string.
    {
        auto tokens = tok.tokenize("'hello'");
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(2));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::String);
        ASSERT_EQ(tokens[0].str_value, "hello");
        ASSERT_EQ(tokens[0].lexeme, "'hello'");
    }

    // Empty string.
    {
        auto tokens = tok.tokenize("''");
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(2));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::String);
        ASSERT_EQ(tokens[0].str_value, "");
    }

    // Escaped quote: '' → '
    {
        auto tokens = tok.tokenize("'it''s'");
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(2));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::String);
        ASSERT_EQ(tokens[0].str_value, "it's");
    }

    // Unterminated string.
    {
        auto tokens = tok.tokenize("'hello");
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(2));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::Error);
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — punctuation and operators
// -----------------------------------------------------------------------

static bool test_tokenizer_punctuation()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("( ) , ; * = < > <= >= <>");
    // 11 punct + End = 12
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(12));
    ASSERT_EQ(tokens[0].kind,  serodb::TokenKind::LeftParen);
    ASSERT_EQ(tokens[1].kind,  serodb::TokenKind::RightParen);
    ASSERT_EQ(tokens[2].kind,  serodb::TokenKind::Comma);
    ASSERT_EQ(tokens[3].kind,  serodb::TokenKind::Semicolon);
    ASSERT_EQ(tokens[4].kind,  serodb::TokenKind::Asterisk);
    ASSERT_EQ(tokens[5].kind,  serodb::TokenKind::Equals);
    ASSERT_EQ(tokens[6].kind,  serodb::TokenKind::Less);
    ASSERT_EQ(tokens[7].kind,  serodb::TokenKind::Greater);
    ASSERT_EQ(tokens[8].kind,  serodb::TokenKind::LessEqual);
    ASSERT_EQ(tokens[9].kind,  serodb::TokenKind::GreaterEqual);
    ASSERT_EQ(tokens[10].kind, serodb::TokenKind::NotEquals);
    ASSERT_EQ(tokens[11].kind, serodb::TokenKind::End);

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — whitespace and comments
// -----------------------------------------------------------------------

static bool test_tokenizer_whitespace_and_comments()
{
    serodb::Tokenizer tok;

    // Extra whitespace between tokens.
    {
        auto tokens = tok.tokenize("  select   *   from   users  ");
        // SELECT * FROM users End = 5
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(5));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_SELECT);
        ASSERT_EQ(tokens[1].kind, serodb::TokenKind::Asterisk);
        ASSERT_EQ(tokens[2].kind, serodb::TokenKind::KW_FROM);
        ASSERT_EQ(tokens[3].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[3].lexeme, "users");
    }

    // Single-line comments.
    {
        auto tokens = tok.tokenize("select -- this is a comment\n* from users");
        // SELECT * FROM users End = 5
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(5));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_SELECT);
        ASSERT_EQ(tokens[1].kind, serodb::TokenKind::Asterisk);
    }

    // Empty input.
    {
        auto tokens = tok.tokenize("");
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(1));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::End);
    }

    // Whitespace-only input.
    {
        auto tokens = tok.tokenize("   \t\n  ");
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(1));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::End);
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — SQL-like statement tokenization
// -----------------------------------------------------------------------

static bool test_tokenizer_sql_statement()
{
    serodb::Tokenizer tok;

    // A realistic INSERT statement.
    {
        auto tokens = tok.tokenize(
            "INSERT INTO users (id, username, email) VALUES (1, 'alice', 'a@b.com')");        // INSERT INTO users (id, username, email) VALUES (1, 'alice', 'a@b.com') End
        // = 19 tokens
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(19));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_INSERT);
        ASSERT_EQ(tokens[1].kind, serodb::TokenKind::KW_INTO);
        ASSERT_EQ(tokens[2].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[2].lexeme, "users");
        ASSERT_EQ(tokens[3].kind, serodb::TokenKind::LeftParen);
        ASSERT_EQ(tokens[4].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[5].kind, serodb::TokenKind::Comma);
        ASSERT_EQ(tokens[6].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[6].lexeme, "username");
        ASSERT_EQ(tokens[9].kind, serodb::TokenKind::RightParen);
        ASSERT_EQ(tokens[10].kind, serodb::TokenKind::KW_VALUES);
        ASSERT_EQ(tokens[11].kind, serodb::TokenKind::LeftParen);
        ASSERT_EQ(tokens[12].kind, serodb::TokenKind::Integer);
        ASSERT_EQ(tokens[12].int_value, 1);
        ASSERT_EQ(tokens[14].kind, serodb::TokenKind::String);
        ASSERT_EQ(tokens[14].str_value, "alice");
        ASSERT_EQ(tokens[16].kind, serodb::TokenKind::String);
        ASSERT_EQ(tokens[16].str_value, "a@b.com");
    }

    // A SELECT with WHERE.
    {
        auto tokens = tok.tokenize(
            "SELECT * FROM users WHERE id = 42;\n");
        // SELECT * FROM users WHERE id = 42 ; End = 10
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(10));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_SELECT);
        ASSERT_EQ(tokens[1].kind, serodb::TokenKind::Asterisk);
        ASSERT_EQ(tokens[2].kind, serodb::TokenKind::KW_FROM);
        ASSERT_EQ(tokens[3].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[4].kind, serodb::TokenKind::KW_WHERE);
        ASSERT_EQ(tokens[5].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[6].kind, serodb::TokenKind::Equals);
        ASSERT_EQ(tokens[7].kind, serodb::TokenKind::Integer);
        ASSERT_EQ(tokens[7].int_value, 42);
        ASSERT_EQ(tokens[8].kind, serodb::TokenKind::Semicolon);
    }

    // CREATE TABLE.
    {
        auto tokens = tok.tokenize(
            "CREATE TABLE employees (id INT, name VARCHAR(50));");
        // CREATE TABLE employees ( id INT , name VARCHAR ( 50 ) ) ; End = 15
        ASSERT_EQ(tokens.size(), static_cast<std::size_t>(15));
        ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_CREATE);
        ASSERT_EQ(tokens[1].kind, serodb::TokenKind::KW_TABLE);
        ASSERT_EQ(tokens[2].kind, serodb::TokenKind::Identifier);
        ASSERT_EQ(tokens[2].lexeme, "employees");
    }

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — error tokens for unrecognized characters
// -----------------------------------------------------------------------

static bool test_tokenizer_errors()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("select @#$ from users");
    // SELECT @ # $ FROM users End
    ASSERT_EQ(tokens.size(), static_cast<std::size_t>(7));
    ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_SELECT);
    ASSERT_EQ(tokens[1].kind, serodb::TokenKind::Error);
    ASSERT_EQ(tokens[1].lexeme, "@");
    ASSERT_EQ(tokens[2].kind, serodb::TokenKind::Error);
    ASSERT_EQ(tokens[2].lexeme, "#");
    ASSERT_EQ(tokens[3].kind, serodb::TokenKind::Error);
    ASSERT_EQ(tokens[3].lexeme, "$");
    ASSERT_EQ(tokens[4].kind, serodb::TokenKind::KW_FROM);

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — source position tracking
// -----------------------------------------------------------------------

static bool test_tokenizer_positions()
{
    serodb::Tokenizer tok;

    auto tokens = tok.tokenize("select\n  id from users");
    // SELECT \n id FROM users End
    ASSERT_EQ(tokens[0].kind, serodb::TokenKind::KW_SELECT);
    ASSERT_EQ(tokens[0].line, static_cast<std::size_t>(1));
    ASSERT_EQ(tokens[0].column, static_cast<std::size_t>(1));

    // 'id' is on line 2, column 3 (after two spaces).
    ASSERT_EQ(tokens[1].kind, serodb::TokenKind::Identifier);
    ASSERT_EQ(tokens[1].lexeme, "id");
    ASSERT_EQ(tokens[1].line, static_cast<std::size_t>(2));
    ASSERT_EQ(tokens[1].column, static_cast<std::size_t>(3));

    return true;
}

// -----------------------------------------------------------------------
// Test: Tokenizer — token_kind_name
// -----------------------------------------------------------------------

static bool test_tokenizer_kind_names()
{
    // Smoke test: every kind returns a non-empty name.
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::Identifier)).size() > 0);
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::Integer)).size() > 0);
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::String)).size() > 0);
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::KW_SELECT)).size() > 0);
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::End)).size() > 0);
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::Error)).size() > 0);
    ASSERT_TRUE(std::string(serodb::token_kind_name(serodb::TokenKind::NotEquals)).size() > 0);

    return true;
}

// -----------------------------------------------------------------------
// Main — run all tests
// -----------------------------------------------------------------------

int main()
{
    std::cout << "========================================\n"
              << "  SeroDB Test Suite\n"
              << "========================================\n\n";

    run_test("Row validation",            test_row_validation);
    run_test("Parser INSERT",             test_parser_insert);
    run_test("Parser SELECT",             test_parser_select);
    run_test("Parser invalid commands",   test_parser_invalid);
    run_test("Parser meta-commands",      test_parser_meta_commands);
    run_test("Constants sanity",          test_constants);
    run_test("Pager round-trip",          test_pager_round_trip);
    run_test("Pager stats",              test_pager_stats);
    run_test("Pager flush",              test_pager_flush);
    run_test("Serialization round-trip",  test_serialization_round_trip);
    run_test("Table insert multiple",     test_table_insert_multiple);
    run_test("Table persistence",         test_table_persistence);
    run_test("Empty database",            test_empty_database);
    run_test("Cursor on empty table",     test_cursor_empty);
    run_test("Cursor traversal",          test_cursor_traversal);
    run_test("Multi-page spanning",       test_multi_page);
    run_test("Executor INSERT + SELECT",  test_executor);
    run_test("Parser email too long",         test_parser_email_too_long);
    run_test("Parser extra trailing text",    test_parser_extra_trailing_text);
    run_test("Parser missing close paren",    test_parser_missing_close_paren);
    run_test("Parser id boundary",            test_parser_id_boundary);
    run_test("Parser id leading zeros",       test_parser_id_leading_zeros);
    run_test("Parser SELECT case-insensitive", test_parser_select_case_insensitive);
    run_test("Row empty fields",              test_row_empty_fields);
    run_test("Row id boundary",               test_row_id_boundary);
    run_test("Table insert invalid row",      test_table_insert_invalid_row);
    run_test("Table insert when full",        test_table_insert_full);
    run_test("Executor SELECT empty",         test_executor_select_empty);
    run_test("Executor multiple INSERTs",     test_executor_multiple_inserts);
    run_test("Executor invalid INSERT",       test_executor_invalid_insert);
    run_test("Serialization format",          test_serialization_format);
    run_test("Table row_slot boundary",       test_table_row_slot_boundary);
    run_test("Tokenizer keywords",             test_tokenizer_keywords);
    run_test("Tokenizer keyword case",         test_tokenizer_keyword_case);
    run_test("Tokenizer identifiers",          test_tokenizer_identifiers);
    run_test("Tokenizer integers",             test_tokenizer_integers);
    run_test("Tokenizer strings",              test_tokenizer_strings);
    run_test("Tokenizer punctuation",          test_tokenizer_punctuation);
    run_test("Tokenizer whitespace/comments",  test_tokenizer_whitespace_and_comments);
    run_test("Tokenizer SQL statement",        test_tokenizer_sql_statement);
    run_test("Tokenizer error tokens",         test_tokenizer_errors);
    run_test("Tokenizer source positions",     test_tokenizer_positions);
    run_test("Tokenizer kind names",           test_tokenizer_kind_names);

    std::cout << "\n========================================\n"
              << "  Results: " << g_tests_passed << "/" << g_tests_run
              << " passed, " << g_tests_failed << " failed\n"
              << "========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
