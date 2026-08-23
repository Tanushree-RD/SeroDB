#pragma once

#include <string>
#include <vector>

#include "serodb/row.hpp"

namespace serodb {

/**
 * Database class – owns the in‑memory collection of rows and provides a
 * high‑level API for loading, saving and inserting data. It delegates the
 * actual binary I/O to StorageManager.
 */
class Database {
public:
    Database() = default;
    explicit Database(const std::string& path) { load(path); }

    // Load rows from a file into memory.
    void load(const std::string& path);

    // Save current rows to a file.
    void save(const std::string& path) const;

    // Insert a new row; throws if the row is invalid.
    void insert(const Row& row);

    // Read‑only access to the stored rows.
    const std::vector<Row>& rows() const noexcept { return rows_; }

private:
    std::vector<Row> rows_;
};

} // namespace serodb
