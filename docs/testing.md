# Testing Strategy

This document describes the test suite, test runner architecture, and execution procedures for SeroDB.

## Running Tests

Tests are compiled into the standalone executable `serodb_tests`.

### Building and Executing

```sh
# Configure and build
cmake -S . -B build
cmake --build build

# Run the test suite directly
./build/serodb_tests        # Linux / macOS
.\build\serodb_tests.exe    # Windows
```

## Test Runner Architecture

SeroDB uses a lightweight, self-contained test runner in `tests/tests.cpp` without external dependencies:

- **Assertion Macros**: `ASSERT_TRUE`, `ASSERT_EQ`, and `ASSERT_NE` report the exact file and line on failure.
- **RAII TempFile**: The `TempFile` helper automatically deletes temporary database files (`.db`) created during test runs upon scope exit, ensuring clean test isolation.
- **Deterministic Execution**: Each test operates on an independent file and resets all state.

## Current Test Cases

The test suite contains 17 comprehensive unit and integration tests:

| Test Name | Category | What is Tested |
|---|---|---|
| `Row validation` | Domain | Validation rules for username length, email length, and valid field boundaries |
| `Parser INSERT` | Parsing | Successful parsing of `insert (id, username, email)` syntax |
| `Parser SELECT` | Parsing | Variations of `select`, `select *`, case insensitivity, whitespace trimming |
| `Parser invalid commands` | Parsing | Handling of empty input, unknown commands, malformed IDs, string overflow |
| `Parser meta-commands` | Parsing | Recognition of `.exit`, `.help`, `.tables`, `.constants`, `.stats`, and unrecognized commands |
| `Constants sanity` | Configuration | Layout assertions: row size, page size, header size, rows-per-page geometry |
| `Pager round-trip` | Pager | Writing raw page bytes to disk, reopening file, and verifying exact byte match |
| `Pager stats` | Pager | Verifying lazy page allocation and `pages_loaded()` count behavior |
| `Pager flush` | Pager | Verifying that `flush()` persists pages directly to disk |
| `Serialization round-trip` | Storage | Inserting a row via `Table`, reopening file, reading with `Cursor`, verifying field match |
| `Table insert multiple` | Table | Inserting multiple rows, closing table, reopening, verifying row count and contents |
| `Table persistence` | Table | Multi-stage restarts: inserting batches of rows across multiple open/close cycles |
| `Empty database` | Table | Opening a new file and verifying 0 rows and immediate end-of-cursor |
| `Cursor on empty table` | Cursor | Verifying `table_start()` equals `table_end()` when no rows exist |
| `Cursor traversal` | Cursor | Sequential scan of rows with verification of `row_num()` and data integrity |
| `Multi-page spanning` | Paging | Inserting enough rows to span across multiple 4096-byte pages (Page 0, 1, 2) |
| `Executor INSERT + SELECT` | Execution | End-to-end execution of statements through `Executor` with stream verification |

## Future Testing Strategy

As new milestones are implemented, the testing suite will expand into:

1. **B-Tree Invariant Testing**: Verification that keys remain sorted across splits, parent pointers are correctly maintained, and balancing logic holds after node splits and merges.
2. **Crash & Recovery Testing**: Inducing artificial crashes before page flushes or midway through writes to test WAL replay and transaction rollback.
3. **Fuzz Testing**: Feeding randomized byte streams to the Parser and Pager to ensure robust handling of corrupted database files.
4. **Stress & Limit Testing**: Validating table capacity boundary conditions up to `MAX_ROWS` and `MAX_PAGES`.
