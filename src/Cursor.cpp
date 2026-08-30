#include "serodb/Cursor.hpp"

#include "serodb/Table.hpp"
#include "serodb/row.hpp"

namespace serodb {

// -----------------------------------------------------------------------
// Factory functions
// -----------------------------------------------------------------------

Cursor Cursor::table_start(Table& table)
{
    // Point to row 0.  If the table is empty, this is also "end".
    return Cursor(table, 0, table.row_count() == 0);
}

Cursor Cursor::table_end(Table& table)
{
    // Point one past the last row (the append position).
    return Cursor(table, table.row_count(), true);
}

// -----------------------------------------------------------------------
// Navigation
// -----------------------------------------------------------------------

void Cursor::advance()
{
    ++row_num_;
    if (row_num_ >= table_.row_count()) {
        end_of_table_ = true;
    }
}

// -----------------------------------------------------------------------
// Data access
// -----------------------------------------------------------------------

char* Cursor::value()
{
    const auto slot = table_.row_slot(row_num_);
    char* page = table_.pager_.get_page(slot.first);
    return page + slot.second;
}

Row Cursor::read() const
{
    const auto slot = table_.row_slot(row_num_);
    const char* page = table_.pager_.get_page(slot.first);
    return Table::deserialize_row(page + slot.second);
}

void Cursor::write(const Row& row)
{
    const auto slot = table_.row_slot(row_num_);
    char* page = table_.pager_.get_page(slot.first);
    Table::serialize_row(row, page + slot.second);
}

// -----------------------------------------------------------------------
// Private constructor
// -----------------------------------------------------------------------

Cursor::Cursor(Table& table, std::size_t row_num, bool is_end)
    : table_(table)
    , row_num_(row_num)
    , end_of_table_(is_end)
{
}

} // namespace serodb
