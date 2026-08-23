#pragma once

#include <string>
#include <vector>

#include "serodb/row.hpp"

namespace serodb {

/**
 * StorageManager – low‑level binary persistence for SeroDB.
 * Provides static helpers that read/write a vector of Row objects to a file.
 */
class StorageManager {
public:
    /** Load rows from a binary database file.
     *  Returns an empty vector if the file does not exist.
     *  Throws std::runtime_error on format errors.
     */
    static std::vector<Row> load(const std::string& path);

    /** Save rows to a binary database file.
     *  Throws std::runtime_error on I/O failures.
     */
    static void save(const std::string& path, const std::vector<Row>& rows);
};

} // namespace serodb
