# Binary Serialization

This document details how rows and numerical values are encoded into raw binary format for on-disk persistence.

## Row Binary Layout

Each row is serialized into a fixed-width buffer of exactly **291 bytes**.

```
+----------------+--------------------------+--------------------------+
| id             | username                 | email                    |
| Offset: 0      | Offset: 4                | Offset: 36               |
| Size: 4 bytes  | Size: 32 bytes           | Size: 255 bytes          |
| uint32 LE      | null-terminated / padded | null-terminated / padded |
+----------------+--------------------------+--------------------------+
```

### Field Breakdown

| Field | Byte Offset | Size (Bytes) | Format | Description |
|---|---|---|---|---|
| `id` | 0 | 4 | `uint32` (Little-Endian) | Unique unsigned 32-bit row identifier |
| `username` | 4 | 32 | `char[32]` | Fixed-width string, null-terminated and zero-padded |
| `email` | 36 | 255 | `char[255]` | Fixed-width string, null-terminated and zero-padded |

Total size: `4 + 32 + 255 = 291` bytes.

## Endianness Encoding

All integer values (`id` and the table header's `row_count`) are explicitly encoded and decoded using little-endian byte ordering. This guarantees byte-for-byte binary portability across systems regardless of the host CPU architecture.

### Little-Endian Integer Writing

```cpp
void write_u32_le(char* dest, std::uint32_t value)
{
    dest[0] = static_cast<char>(value & 0xFFu);
    dest[1] = static_cast<char>((value >> 8u) & 0xFFu);
    dest[2] = static_cast<char>((value >> 16u) & 0xFFu);
    dest[3] = static_cast<char>((value >> 24u) & 0xFFu);
}
```

### Little-Endian Integer Reading

```cpp
std::uint32_t read_u32_le(const char* src)
{
    auto byte = [&](int i) -> std::uint32_t {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(src[i]));
    };
    return byte(0) | (byte(1) << 8u) | (byte(2) << 16u) | (byte(3) << 24u);
}
```

## String Serialization

Fixed-width string fields are serialized by zero-filling the designated buffer width and copying the string characters:

```cpp
void write_fixed_string(char* dest, const std::string& value, std::size_t width)
{
    std::memset(dest, 0, width);
    std::memcpy(dest, value.data(), std::min(value.size(), width));
}
```

When reading strings from binary buffers, `std::memchr` is used to locate the first null terminator byte (`\0`), or the full width is taken if the field is completely filled:

```cpp
std::string read_fixed_string(const char* src, std::size_t width)
{
    const char* end = static_cast<const char*>(std::memchr(src, '\0', width));
    const std::size_t len = end ? static_cast<std::size_t>(end - src) : width;
    return std::string(src, len);
}
```

## Serialization Safety and Validation

Before serializing a row, `Table::insert` verifies constraints:
- `row.is_valid()` must return true (`username.size() <= 32` and `email.size() <= 255`).
- Unused trailing bytes in fixed string slots are guaranteed to be zeroed out, preventing data leakage from previous memory allocations.
