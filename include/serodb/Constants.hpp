#pragma once

/**
 * Constants.hpp — Centralized configuration for the SeroDB storage engine.
 *
 * These constants mirror the fundamental sizing decisions that every database
 * engine must make.  In SQLite the page size defaults to 4096 bytes and the
 * database header occupies the first 100 bytes of page 1.  SeroDB follows the
 * same philosophy at a smaller scale.
 */

#include <array>
#include <cstddef>
#include <cstdint>

namespace serodb {

// ---------------------------------------------------------------------------
// File header
// ---------------------------------------------------------------------------

/// Magic bytes written at the start of every SeroDB database file.
/// Used to identify the file format and reject corrupt or foreign files.
constexpr std::array<char, 8> FILE_MAGIC = {'S', 'e', 'r', 'o', 'D', 'B', '1', '\0'};

/// Total size of the file header that precedes row data.
/// Layout: 8‑byte magic  |  4‑byte row‑count (little‑endian uint32).
constexpr std::size_t HEADER_SIZE = FILE_MAGIC.size() + sizeof(std::uint32_t); // 12

// ---------------------------------------------------------------------------
// Page geometry
// ---------------------------------------------------------------------------

/// Every database file is divided into fixed‑size pages.
/// The pager reads and writes whole pages — never individual bytes.
constexpr std::size_t PAGE_SIZE = 4096;

/// Hard limit on the number of pages a single database file may contain.
/// Keeps memory usage bounded and simplifies the page‑cache array.
constexpr std::size_t MAX_PAGES = 100;

// ---------------------------------------------------------------------------
// Row geometry
// ---------------------------------------------------------------------------

/// Serialized size of a single Row on disk.
///   4 bytes  — id        (uint32, little‑endian)
///  32 bytes  — username  (fixed‑width, null‑padded)
/// 255 bytes  — email     (fixed‑width, null‑padded)
/// Total: 291 bytes.
constexpr std::size_t SERIALIZED_ROW_SIZE = sizeof(std::uint32_t)
                                          + 32   // Row::max_username_length
                                          + 255; // Row::max_email_length

/// Page 0 holds the file header, so it has fewer usable bytes for row data.
constexpr std::size_t PAGE_0_BODY = PAGE_SIZE - HEADER_SIZE;

/// Number of rows that fit into page 0 (after the header).
constexpr std::size_t ROWS_IN_PAGE_0 = PAGE_0_BODY / SERIALIZED_ROW_SIZE; // 14

/// Number of rows that fit into every subsequent page (no header).
constexpr std::size_t ROWS_PER_FULL_PAGE = PAGE_SIZE / SERIALIZED_ROW_SIZE; // 14

/// Maximum number of rows the database can hold.
/// Page 0 contributes ROWS_IN_PAGE_0, the remaining pages each contribute
/// ROWS_PER_FULL_PAGE.
constexpr std::size_t MAX_ROWS = ROWS_IN_PAGE_0 + (MAX_PAGES - 1) * ROWS_PER_FULL_PAGE;

// ---------------------------------------------------------------------------
// Default paths
// ---------------------------------------------------------------------------

constexpr const char* DEFAULT_DATABASE_PATH = "serodb.db";

} // namespace serodb
