#include "serodb/row.hpp"

namespace serodb {

bool Row::is_valid_username(const std::string& value)
{
    return value.size() <= max_username_length;
}

bool Row::is_valid_email(const std::string& value)
{
    return value.size() <= max_email_length;
}

bool Row::is_valid() const
{
    return is_valid_username(username) && is_valid_email(email);
}

} // namespace serodb
