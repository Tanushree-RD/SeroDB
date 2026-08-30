#pragma once

/**
 * Pager.hpp — Page‑level disk I/O for SeroDB.
 *
 * The Pager is the lowest layer in the storage stack.  It treats the database
 * file as a sequence of fixed‑size pages and provides:
 *
 *   • Lazy loading  — pages are read from disk only when first requested.
 *   • In‑memory cache — once loaded a page stays in RAM until flushed.
 *   • Write‑back — dirty pages are flushed individually, not the whole file.
 *
 * Higher layers (Table, Cursor) work exclusively through the Pager; they
 * never touch the file descriptor directly.
 *
 * This design mirrors the sqlite3PagerGet / sqlite3PagerWrite API in SQLite.
 */

#include "serodb/Constants.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>

namespace serodb {

class Pager {
public:
    /// Open (or create) the database file at `filename`.
    explicit Pager(const std::string& filename);

    /// Flushes all loaded pages and closes the file.
    ~Pager();

    // Non‑copyable, non‑movable (owns file handle and page buffers).
    Pager(const Pager&) = delete;
    Pager& operator=(const Pager&) = delete;

    // -----------------------------------------------------------------
    // Page access
    // -----------------------------------------------------------------

    /// Return a pointer to the PAGE_SIZE‑byte buffer for `page_num`.
    /// If the page is not yet cached it is lazily loaded from disk (or
    /// zero‑initialised for pages beyond the current file length).
    /// Throws if `page_num >= MAX_PAGES`.
    char* get_page(std::size_t page_num);

    /// Write `page_num` back to its file offset.
    /// Only pages that have been loaded via get_page() should be flushed.
    void flush(std::size_t page_num);

    /// Flush every loaded page and close the underlying file.
    /// Called automatically by the destructor but may be invoked earlier.
    void close();

    // -----------------------------------------------------------------
    // Stats / diagnostics
    // -----------------------------------------------------------------

    /// Number of pages currently held in the cache.
    std::size_t pages_loaded() const;

    /// Size of the database file when it was opened (before any writes).
    std::size_t file_length() const noexcept { return file_length_; }

private:
    std::string filename_;
    std::fstream file_;
    std::size_t file_length_ = 0;

    /// Page cache — null entries mean "not loaded yet".
    using PageBuffer = std::array<char, PAGE_SIZE>;
    std::array<std::unique_ptr<PageBuffer>, MAX_PAGES> pages_{};
};

} // namespace serodb
