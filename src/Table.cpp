#include "serodb/Table.hpp"

#include "serodb/Constants.hpp"
#include "serodb/Pager.hpp"
#include "serodb/row.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace serodb {

// -----------------------------------------------------------------------
// Little‑endian helpers  (portable, no alignment assumptions)
// -----------------------------------------------------------------------

namespace {

void write_u32_le(char* dest, std::uint32_t value)
{
    dest[0] = static_cast<char>(value & 0xFFu);
    dest[1] = static_cast<char>((value >> 8u) & 0xFFu);
    dest[2] = static_cast<char>((value >> 16u) & 0xFFu);
    dest[3] = static_cast<char>((value >> 24u) & 0xFFu);
}

std::uint32_t read_u32_le(const char* src)
{
    auto byte = [&](int i) -> std::uint32_t {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(src[i]));
    };
    return byte(0) | (byte(1) << 8u) | (byte(2) << 16u) | (byte(3) << 24u);
}

void write_fixed_string(char* dest, const std::string& value, std::size_t width)
{
    std::memset(dest, 0, width);
    std::memcpy(dest, value.data(), std::min(value.size(), width));
}

std::string read_fixed_string(const char* src, std::size_t width)
{
    // Find the first null byte (or use the full width).
    const char* end = static_cast<const char*>(std::memchr(src, '\0', width));
    const std::size_t len = end ? static_cast<std::size_t>(end - src) : width;
    return std::string(src, len);
}

} // anonymous namespace

// -----------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------

Table::Table(const std::string& filename)
    : pager_(filename)
{
    num_rows_ = read_header();
}

Table::~Table()
{
    try {
        close();
    } catch (...) {}
}

void Table::close()
{
    // Persist the current row count in the header before flushing pages.
    write_header();
    pager_.close();
}

// -----------------------------------------------------------------------
// Row operations
// -----------------------------------------------------------------------

void Table::insert(const Row& row)
{
    if (!row.is_valid()) {
        throw std::runtime_error("Table: cannot insert an invalid row");
    }
    if (num_rows_ >= MAX_ROWS) {
        throw std::runtime_error("Table: table is full");
    }

    const auto slot = row_slot(num_rows_);
    char* page = pager_.get_page(slot.first);
    serialize_row(row, page + slot.second);
    ++num_rows_;
}

// -----------------------------------------------------------------------
// Row slot mapping
// -----------------------------------------------------------------------

std::pair<std::size_t, std::size_t> Table::row_slot(std::size_t row_num) const
{
    // Page 0 has HEADER_SIZE bytes reserved at the start, so it fits
    // fewer rows than subsequent pages.
    if (row_num < ROWS_IN_PAGE_0) {
        // Row belongs to page 0.
        const std::size_t offset = HEADER_SIZE + row_num * SERIALIZED_ROW_SIZE;
        return {0, offset};
    }

    // Subtract page‑0 rows, then map linearly across full pages.
    const std::size_t adjusted = row_num - ROWS_IN_PAGE_0;
    const std::size_t page_num = 1 + adjusted / ROWS_PER_FULL_PAGE;
    const std::size_t offset   = (adjusted % ROWS_PER_FULL_PAGE) * SERIALIZED_ROW_SIZE;
    return {page_num, offset};
}

// -----------------------------------------------------------------------
// Serialization
// -----------------------------------------------------------------------

void Table::serialize_row(const Row& row, char* dest)
{
    // Layout: [4‑byte id] [32‑byte username] [255‑byte email]
    write_u32_le(dest, row.id);
    write_fixed_string(dest + sizeof(std::uint32_t),
                       row.username, Row::max_username_length);
    write_fixed_string(dest + sizeof(std::uint32_t) + Row::max_username_length,
                       row.email, Row::max_email_length);
}

Row Table::deserialize_row(const char* src)
{
    Row row;
    row.id       = read_u32_le(src);
    row.username  = read_fixed_string(src + sizeof(std::uint32_t),
                                      Row::max_username_length);
    row.email     = read_fixed_string(src + sizeof(std::uint32_t) + Row::max_username_length,
                                      Row::max_email_length);
    return row;
}

// -----------------------------------------------------------------------
// Header I/O
// -----------------------------------------------------------------------

std::size_t Table::read_header()
{
    if (pager_.file_length() == 0) {
        // Brand‑new file — no header yet.
        return 0;
    }

    char* page0 = pager_.get_page(0);

    // Verify magic bytes.
    if (std::memcmp(page0, FILE_MAGIC.data(), FILE_MAGIC.size()) != 0) {
        throw std::runtime_error("Table: invalid SeroDB database file header");
    }

    const std::uint32_t count = read_u32_le(page0 + FILE_MAGIC.size());
    return static_cast<std::size_t>(count);
}

void Table::write_header()
{
    char* page0 = pager_.get_page(0);

    // Write magic.
    std::memcpy(page0, FILE_MAGIC.data(), FILE_MAGIC.size());

    // Write row count.
    write_u32_le(page0 + FILE_MAGIC.size(),
                 static_cast<std::uint32_t>(num_rows_));
}

} // namespace serodb
