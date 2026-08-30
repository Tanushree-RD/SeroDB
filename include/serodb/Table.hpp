#pragma once

/**
 * Table.hpp — Row‑level storage abstraction for SeroDB.
 *
 * A Table manages a collection of rows stored across pages.  It owns a Pager
 * and handles:
 *
 *   • Row serialisation / deserialisation into page buffers.
 *   • Mapping a logical row number to a (page, byte‑offset) slot.
 *   • Reading / writing the file header (magic + row count) on page 0.
 *   • Providing insert / row‑count operations for higher layers.
 *
 * Rows are stored sequentially, packed tightly within each page.  A row is
 * never split across a page boundary — if it doesn't fit in the remaining
 * space the next page is used.  Page 0 is special because the first
 * HEADER_SIZE bytes hold the file header.
 *
 * This design mirrors the sqlite3BtreeInsert / sqlite3BtreeCursor model,
 * though it uses a flat layout rather than a B‑Tree (B‑Tree comes later).
 */

#include "serodb/Constants.hpp"
#include "serodb/Pager.hpp"
#include "serodb/row.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace serodb {

// Forward‑declare Cursor so we can make it a friend.
class Cursor;

class Table {
public:
    /// Open (or create) a table backed by the database file at `filename`.
    explicit Table(const std::string& filename);

    /// Flush all pages and close the underlying file.
    ~Table();

    // Non‑copyable (owns a Pager).
    Table(const Table&) = delete;
    Table& operator=(const Table&) = delete;

    // -----------------------------------------------------------------
    // Row operations
    // -----------------------------------------------------------------

    /// Append a row to the end of the table.
    /// Throws on validation failure or when the table is full.
    void insert(const Row& row);

    /// Number of rows currently stored.
    std::size_t row_count() const noexcept { return num_rows_; }

    /// Flush all dirty pages and close the database file.
    void close();

    // -----------------------------------------------------------------
    // Pager access (for stats / diagnostics)
    // -----------------------------------------------------------------

    Pager& pager() noexcept { return pager_; }
    const Pager& pager() const noexcept { return pager_; }

private:
    friend class Cursor;

    // -----------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------

    /// Map a logical row number to (page_num, byte_offset_within_page).
    std::pair<std::size_t, std::size_t> row_slot(std::size_t row_num) const;

    /// Serialize `row` into `dest` (exactly SERIALIZED_ROW_SIZE bytes).
    static void serialize_row(const Row& row, char* dest);

    /// Deserialize a row from `src` (exactly SERIALIZED_ROW_SIZE bytes).
    static Row deserialize_row(const char* src);

    /// Read the file header from page 0 and return the stored row count.
    /// Returns 0 for a brand‑new (empty) file.
    std::size_t read_header();

    /// Write the current row count into the header on page 0.
    void write_header();

    // -----------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------

    Pager pager_;
    std::size_t num_rows_ = 0;
};

} // namespace serodb
