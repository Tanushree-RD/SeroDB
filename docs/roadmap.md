# Development Roadmap

This document outlines the milestones for SeroDB. The project is implemented in logical increments to build a complete database engine from first principles.

## Milestone Status

| Milestone | Status | Description |
|---|---|---|
| **1. REPL & Memory Row Representation** | Done | Command prompt, in-memory row structures, input reading |
| **2. Binary Persistence** | Done | File header format, serialization, and disk storage |
| **3. Command Parsing & Statements** | Done | `prepareStatement`, `StatementType`, syntax and limit validation |
| **4. Pager, Table & Cursor Architecture** | Done | 4096-byte pages, lazy caching, row slot mapping, and forward cursor |
| **5. Leaf Node B-Tree** | Planned | B-Tree leaf node format, key searching, node splitting |
| **6. Internal Node B-Tree** | Planned | Multi-level B-Tree with internal routing nodes and parent pointers |
| **7. SQL Tokenizer & Parser** | Planned | Proper SQL lexer and AST parser for expressions |
| **8. WHERE Clause & Filtering** | Planned | Predicate evaluation on cursor scans and indexed lookups |
| **9. UPDATE & DELETE Operations** | Planned | In-place updates, tombstone/cell deletion, and free space reclamation |
| **10. Multi-Table Catalog** | Planned | Schema management and system catalog for multiple tables |
| **11. Transactions & ACID** | Planned | Atomic operations, isolation levels, and rollback mechanism |
| **12. Write-Ahead Logging (WAL)** | Planned | Append-only log for durability and fast recovery |

## Detailed Checklist

- [x] Basic REPL interface
- [x] Fixed-size binary row serialization
- [x] File persistence across restarts
- [x] Structured statement parsing
- [x] Meta-command subsystem (`.exit`, `.help`, `.tables`, `.constants`, `.stats`)
- [x] 4096-byte page-based I/O (`Pager`)
- [x] In-memory page caching and dirty-page flushing
- [x] Table abstraction with slot mapping across pages
- [x] Cursor forward-traversal abstraction
- [ ] B-Tree leaf node format
- [ ] B-Tree key lookup (binary search in page)
- [ ] B-Tree node splitting and root replacement
- [ ] B-Tree internal nodes and tree traversal
- [ ] SQL lexer and tokenizer
- [ ] Query planner (table scan vs index search)
- [ ] WHERE filter predicates
- [ ] In-place row update
- [ ] Row deletion and page rebalancing
- [ ] Table schema metadata
- [ ] Transaction begin / commit / rollback
- [ ] Write-ahead log (WAL) and recovery manager
