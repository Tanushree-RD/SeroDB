#include "serodb/storage.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace serodb {
namespace {

constexpr std::array<char, 8> file_magic = {'S', 'e', 'r', 'o', 'D', 'B', '1', '\0'};
constexpr std::size_t id_size = sizeof(std::uint32_t);
constexpr std::size_t username_size = Row::max_username_length;
constexpr std::size_t email_size = Row::max_email_length;

void write_u32_le(std::ostream& output, std::uint32_t value)
{
    const std::array<char, 4> bytes = {
        static_cast<char>(value & 0xffu),
        static_cast<char>((value >> 8u) & 0xffu),
        static_cast<char>((value >> 16u) & 0xffu),
        static_cast<char>((value >> 24u) & 0xffu),
    };

    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool read_u32_le(std::istream& input, std::uint32_t& value)
{
    std::array<unsigned char, 4> bytes{};
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    if (!input) {
        return false;
    }

    value = static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8u)
        | (static_cast<std::uint32_t>(bytes[2]) << 16u)
        | (static_cast<std::uint32_t>(bytes[3]) << 24u);

    return true;
}

void write_fixed_string(std::ostream& output, const std::string& value, std::size_t width)
{
    if (value.size() > width) {
        throw std::runtime_error("row contains a string that is too long to serialize");
    }

    std::vector<char> buffer(width, '\0');
    std::copy(value.begin(), value.end(), buffer.begin());
    output.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
}

std::string read_fixed_string(std::istream& input, std::size_t width)
{
    std::vector<char> buffer(width, '\0');
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    if (!input) {
        throw std::runtime_error("database file ended in the middle of a row");
    }

    const auto terminator = std::find(buffer.begin(), buffer.end(), '\0');
    return std::string(buffer.begin(), terminator);
}

void write_row(std::ostream& output, const Row& row)
{
    if (!row.is_valid()) {
        throw std::runtime_error("cannot serialize an invalid row");
    }

    write_u32_le(output, row.id);
    write_fixed_string(output, row.username, username_size);
    write_fixed_string(output, row.email, email_size);
}

Row read_row(std::istream& input)
{
    std::uint32_t id{};

    if (!read_u32_le(input, id)) {
        throw std::runtime_error("database file ended in the middle of a row");
    }

    Row row;
    row.id = id;
    row.username = read_fixed_string(input, username_size);
    row.email = read_fixed_string(input, email_size);

    if (!row.is_valid()) {
        throw std::runtime_error("database file contains an invalid row");
    }

    return row;
}

} // namespace

std::size_t serialized_row_size()
{
    return id_size + username_size + email_size;
}

std::vector<Row> load_rows_from_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);

    if (!input) {
        return {};
    }

    std::array<char, file_magic.size()> magic{};
    input.read(magic.data(), static_cast<std::streamsize>(magic.size()));

    if (!input || magic != file_magic) {
        throw std::runtime_error("invalid SeroDB database file header");
    }

    std::uint32_t row_count{};
    if (!read_u32_le(input, row_count)) {
        throw std::runtime_error("database file is missing its row count");
    }

    std::vector<Row> rows;
    rows.reserve(row_count);

    for (std::uint32_t i = 0; i < row_count; ++i) {
        rows.push_back(read_row(input));
    }

    char trailing_byte{};
    if (input.read(&trailing_byte, 1)) {
        throw std::runtime_error("database file contains trailing bytes");
    }

    return rows;
}

void save_rows_to_file(const std::string& path, const std::vector<Row>& rows)
{
    if (rows.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::runtime_error("too many rows to save");
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);

    if (!output) {
        throw std::runtime_error("could not open database file for writing");
    }

    output.write(file_magic.data(), static_cast<std::streamsize>(file_magic.size()));
    write_u32_le(output, static_cast<std::uint32_t>(rows.size()));

    for (const auto& row : rows) {
        write_row(output, row);
    }

    if (!output) {
        throw std::runtime_error("failed while writing database file");
    }
}

} // namespace serodb
