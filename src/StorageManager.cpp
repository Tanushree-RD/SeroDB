#include "serodb/StorageManager.hpp"

#include "serodb/storage.hpp"

#include <string>
#include <vector>

namespace serodb {

std::vector<Row> StorageManager::load(const std::string& path)
{
    return load_rows_from_file(path);
}

void StorageManager::save(const std::string& path, const std::vector<Row>& rows)
{
    save_rows_to_file(path, rows);
}

} // namespace serodb
