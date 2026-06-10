# std::expected<T, E> API (v0.4.1+)

C++23 type-safe error handling using `std::expected`. Replaces exceptions
and `bool` returns for APIs that can fail.

## Header

```cpp
#include "utils/Expected.h"
```

## Namespace

```cpp
namespace OmniNet {
    template<typename T, typename E>
    using Result = std::expected<T, E>;

    template<typename E>
    using VoidResult = std::expected<void, E>;

    enum class SocketError { ... };
    enum class DbError { ... };

    std::string_view toString(SocketError err);
    std::string_view toString(DbError err);
}
```

## Why std::expected?

| Approach | Performance | Type Safety | Overhead |
|----------|-------------|--------------|----------|
| Exceptions | Slow on error path (~1µs) | Type-safe | Stack unwinding |
| `bool` return | Fast | Not type-safe | Lose error info |
| `std::optional` | Fast | No error info | Must check separately |
| **`std::expected`** | **Fast on all paths** | **Type-safe** | **No overhead** |

From `benchmarks/expected_overhead_bench`:
- **Error path**: `std::expected` is **49x faster** than exceptions
- **Success path**: `std::expected` is slightly slower (20ns vs 4ns) due to
  expected object construction

## Quick Start

```cpp
#include "utils/Expected.h"
using namespace OmniNet;

// Function that can fail
Result<int, SocketError> tryCompute(int x) {
    if (x < 0) {
        return std::unexpected<SocketError>{SocketError::InvalidArgument};
    }
    return x * 2;  // Success value
}

// Caller
int main() {
    auto result = tryCompute(42);
    if (!result) {
        std::cerr << "Failed: " << toString(result.error()) << "\n";
        return 1;
    }
    std::cout << "Result: " << *result << "\n";
    return 0;
}
```

## Error Enums

### SocketError

```cpp
enum class SocketError {
    Success,
    Unknown,
    InvalidAddress,
    ConnectionRefused,
    ConnectionReset,
    Timeout,
    NetworkUnreachable,
    HostUnreachable,
    PermissionDenied,
    AddressInUse,
    OutOfMemory,
    NotConnected,
    AlreadyConnected,
    InProgress,
    WouldBlock,
    MessageTooLong,
    PipeBroken,
    NotASocket,
    InvalidSocket,
    SystemError,
};
```

### DbError

```cpp
enum class DbError {
    Success,
    Unknown,
    NotOpen,
    AlreadyOpen,
    InvalidPath,
    InvalidSql,
    StatementFailed,
    StepFailed,
    BindFailed,
    TypeMismatch,
    OutOfMemory,
    Busy,            // SQLITE_BUSY
    Locked,          // SQLITE_LOCKED
    ReadOnly,        // SQLITE_READONLY
    ConstraintViolation,  // SQLITE_CONSTRAINT
    NoSuchTable,
    NoSuchColumn,
    CorruptDatabase,
    SystemError,
};
```

## API Patterns

### Result<T, E>

```cpp
Result<int, SocketError> result = tryCompute(42);

if (result.has_value()) {
    int value = *result;        // or result.value()
    int& ref = result.value();   // mutable reference
} else {
    SocketError err = result.error();
}
```

### VoidResult<E> (for void operations)

```cpp
VoidResult<DbError> result = db.tryOpen("/data/app.db");

if (result.has_value()) {
    // Success - no value to extract
} else {
    DbError err = result.error();
}
```

### Chaining Operations

```cpp
auto result = openFile()
    .and_then([](File& f) { return readData(f); })
    .or_else([](SocketError) { return openDefault(); });

if (result) {
    process(*result);
}
```

### Early Return on Error

```cpp
Task<void> process(Database& db) {
    auto opened = co_await db.tryOpen_async(path);
    if (!opened) {
        log("DB open failed: %s", toString(opened.error()));
        co_return;
    }
    // ... use db ...
    co_return;
}
```

## Available APIs

### Database (v0.4.1+)

```cpp
class Database {
    // Old void APIs (backward compat)
    void open(const std::string& path);
    void execute(const std::string& sql);
    std::optional<std::vector<std::string>> queryOne(const std::string& sql);
    
    // New std::expected APIs
    [[nodiscard]] Result<void, DbError> tryOpen(const std::string& path);
    [[nodiscard]] Result<void, DbError> tryExecute(const std::string& sql);
    [[nodiscard]] Result<std::optional<std::vector<std::string>>, DbError>
        tryQueryOne(const std::string& sql);
};
```

### Socket (v0.4.1+)

```cpp
class Socket {
    // Old void APIs (backward compat)
    virtual void connect(const std::string& host, uint16_t port);
    virtual void bind(const std::string& address, uint16_t port);
    
    // New std::expected APIs
    [[nodiscard]] virtual Result<void, SocketError>
        tryConnect(const std::string& host, uint16_t port);
    [[nodiscard]] virtual Result<void, SocketError>
        tryBind(const std::string& address, uint16_t port);
};
```

## Migration Guide

### From Exceptions

**Before**:
```cpp
try {
    db.open(path);
    auto rows = db.query(sql);
    process(rows);
} catch (const DatabaseException& e) {
    log("DB error: %s", e.what());
}
```

**After**:
```cpp
if (auto r = db.tryOpen(path); !r) {
    log("DB error: %s", toString(r.error()));
    return;
}
auto rows = db.query(sql);
process(rows);
```

### From `bool` Return

**Before**:
```cpp
if (!socket.bind(addr, port)) {
    log("Bind failed");
    return;
}
```

**After**:
```cpp
auto r = socket.tryBind(addr, port);
if (!r) {
    log("Bind failed: %s", toString(r.error()));
    return;
}
```

### From `std::optional`

**Before**:
```cpp
auto result = db.queryOne(sql);
if (!result) {
    log("No rows found");
    return;
}
process(*result);
// What if there was an error vs no rows? Hard to tell!
```

**After**:
```cpp
auto result = db.tryQueryOne(sql);
if (!result) {
    log("DB error: %s", toString(result.error()));
    return;
}
if (!result->has_value()) {
    log("No rows found");
    return;
}
process(**result);
```

## Best Practices

### 1. Use `[[nodiscard]]`

All new `try*()` methods are marked `[[nodiscard]]`:

```cpp
auto r = db.tryOpen(path);  // Warning if result ignored
if (!r) { /* handle */ }
```

### 2. Don't Unwrap Without Checking

```cpp
// BAD: crashes if error
int value = tryCompute(-1).value();

// GOOD: explicit error handling
auto r = tryCompute(-1);
if (!r) {
    // handle error
    return;
}
int value = *r;
```

### 3. Use `and_then` for Monadic Composition

```cpp
auto result = parseInput(input)
    .and_then([](int x) { return tryCompute(x); })
    .or_else([](SocketError) { return fallbackValue(); });
```

### 4. Add New Errors to Enums

If you have a new error case, add it to the relevant enum:

```cpp
enum class SocketError {
    // ... existing ...
    MyNewError,  // Add new variant
};
```

## See Also

- `tests/test_expected.cpp` - 18 test cases
- `benchmarks/expected_overhead_bench.cpp` - Performance validation
- [C++23 std::expected reference](https://en.cppreference.com/w/cpp/utility/expected)
