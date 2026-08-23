#pragma once

#include "serodb/Statement.hpp"

#include <string>

namespace serodb {

/**
 * Parser – encapsulates command‑line parsing logic.
 * Returns a ParseResult and fills a Statement structure.
 */
class Parser {
public:
    ParseResult parse(const std::string& input, Statement& outStatement) const;
private:
    bool parse_id(const std::string& token, std::uint32_t& id) const;
};

} // namespace serodb
