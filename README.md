# SeroDB

SeroDB is a SQLite-inspired database engine written in modern C++ as a long-term systems programming learning project.

The goal is to understand how a database works internally by building each layer deliberately:

- REPL
- Row representation
- Persistent storage
- Binary serialization
- Simple command parser
- SQL tokenizer
- SQL parser
- Table abstraction
- Pager
- B+ tree indexes
- Transactions

This repository currently contains only the project structure and CMake configuration. Database logic will be added milestone by milestone.

## Requirements

- CMake 3.16 or newer
- A C++17-compatible compiler

## Configure

```sh
cmake -S . -B build
```

## Build

```sh
cmake --build build
```

## Test

```sh
ctest --test-dir build
```

There are no tests yet because no database behavior has been implemented.

