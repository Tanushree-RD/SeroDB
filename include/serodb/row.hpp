#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace serodb {

struct Row {
    static constexpr std::size_t max_username_length = 32;
    static constexpr std::size_t max_email_length = 255;

    std::uint32_t id{};
    std::string username;
    std::string email;

    static bool is_valid_username(const std::string& value);
    static bool is_valid_email(const std::string& value);
    bool is_valid() const;
};

} // namespace serodb
