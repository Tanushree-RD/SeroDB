#pragma once

#include "serodb/row.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace serodb {

inline constexpr const char* default_database_path = "serodb.db";

std::size_t serialized_row_size();
std::vector<Row> load_rows_from_file(const std::string& path);
void save_rows_to_file(const std::string& path, const std::vector<Row>& rows);

} // namespace serodb
