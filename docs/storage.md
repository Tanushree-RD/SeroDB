# Storage Format and Pager Layer

This document details the on-disk storage layout, page geometry, slot calculation, and I/O model used by SeroDB.

## Page Geometry

The database file is divided into uniform 4096-byte pages (`PAGE_SIZE = 4096`). Pages are indexed starting from page 0.

```
+-------------------------------------------------------------+
| Page 0 (4096 bytes)                                         |
| +---------------------------------------------------------+ |
| | Header (12 bytes)                                       | |
| | - Magic bytes: "SeroDB1\0" (8 bytes)                    | |
| | - Row count: uint32 LE (4 bytes)                        | |
| +---------------------------------------------------------+ |
| | Row 0 (291 bytes)                                       | |
| | Row 1 (291 bytes)                                       | |
| | ...                                                     | |
| | Row 13 (291 bytes)                                      | |
| | [Unused padding: 22 bytes]                              | |
| +---------------------------------------------------------+ |
+-------------------------------------------------------------+
| Page 1 (4096 bytes)                                         |
| +---------------------------------------------------------+ |
| | Row 14 (291 bytes)                                      | |
| | Row 15 (291 bytes)                                      | |
| | ...                                                     | |
| | Row 27 (291 bytes)                                      | |
| | [Unused padding: 22 bytes]                              | |
| +---------------------------------------------------------+ |
+-------------------------------------------------------------+
```

### Geometry Constants

| Constant | Value | Description |
|---|---|---|
| `PAGE_SIZE` | 4096 bytes | Size of every disk page block |
| `HEADER_SIZE` | 12 bytes | File header size on Page 0 |
| `SERIALIZED_ROW_SIZE` | 291 bytes | Fixed size of one serialized row |
| `ROWS_IN_PAGE_0` | 14 rows | `(4096 - 12) / 291` |
| `ROWS_PER_FULL_PAGE` | 14 rows | `4096 / 291` |
| `MAX_PAGES` | 100 pages | Maximum pages supported in cache |
| `MAX_ROWS` | 1400 rows | Total table capacity limit |

## Page 0 Header

Page 0 contains metadata required to open and validate the database:

- **Magic String (8 bytes)**: ASCII string `{'S', 'e', 'r', 'o', 'D', 'B', '1', '\0'}`. Used on open to verify file format integrity.
- **Row Count (4 bytes)**: Little-endian 32-bit unsigned integer representing the total number of valid rows stored in the table.

## Row Slot Calculation

A row is never split across a page boundary. If a row does not fit in the remainder of a page, it starts at the beginning of the next page.

The `Table::row_slot(row_num)` method maps any logical row index `row_num` to a `(page_num, byte_offset)` pair:

```cpp
std::pair<std::size_t, std::size_t> Table::row_slot(std::size_t row_num) const
{
    if (row_num < ROWS_IN_PAGE_0) {
        const std::size_t offset = HEADER_SIZE + row_num * SERIALIZED_ROW_SIZE;
        return {0, offset};
    }

    const std::size_t adjusted = row_num - ROWS_IN_PAGE_0;
    const std::size_t page_num = 1 + adjusted / ROWS_PER_FULL_PAGE;
    const std::size_t offset   = (adjusted % ROWS_PER_FULL_PAGE) * SERIALIZED_ROW_SIZE;
    return {page_num, offset};
}
```

## Pager Layer Implementation

The `Pager` class handles all disk I/O and caching:

1. **Lazy Loading**: When `get_page(page_num)` is invoked:
   - If the page is already cached in memory (`pages_[page_num] != nullptr`), the cached pointer is returned immediately.
   - If not cached, memory is allocated, and the corresponding 4096-byte block is read from disk at offset `page_num * PAGE_SIZE`.
   - If the requested page is past the physical end of file, a zeroed buffer is prepared.
2. **Selective Flushing**:
   - `flush(page_num)` writes only the specified 4096-byte page back to disk.
   - `close()` flushes all cached pages and closes the file stream.

## Future Evolution: B-Tree Layer

The current flat array of rows in fixed pages is the foundational step towards a B-Tree indexing structure:
- **Leaf Nodes**: Pages will store cell pointers, keys, and row values with internal node headers instead of linear row slots.
- **Internal Nodes**: Pages will store child page numbers and routing keys.
- The `Pager` API (`get_page`, `flush`) remains identical when transitioning to B-Trees.
