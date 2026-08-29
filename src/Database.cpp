#include "serodb/Database.hpp"

#include "serodb/StorageManager.hpp"
#include "serodb/row.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace serodb {

void Database::load(const std::string& path)
{
    rows_ = StorageManager::load(path);
}

void Database::save(const std::string& path) const
{
    StorageManager::save(path, rows_);
}

void Database::insert(const Row& row)
{
    if (!row.is_valid()) {
        throw std::runtime_error("cannot insert an invalid row");
    }
    rows_.push_back(row);
}

} // namespace serodb
