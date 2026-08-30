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
#include "serodb/row.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
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

    std::cout << "\n========================================\n"
              << "  Results: " << g_tests_passed << "/" << g_tests_run
              << " passed, " << g_tests_failed << " failed\n"
              << "========================================\n";

    return g_tests_failed > 0 ? 1 : 0;
}
