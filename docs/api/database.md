# OmniNet Database API

## Overview

The `OmniNet::DB` namespace provides a thread-safe SQLite3 wrapper with:
- Synchronous and async operations
- Prepared statements with parameter binding
- Transaction support with automatic rollback
- Schema migration system
- Connection pooling (planned)

## Quick Start

```cpp
#include "OmniNet.h"

using namespace OmniNet::DB;

int main() {
    OmniNet::init();
    
    Database db;
    db.open("myapp.db");
    
    // Create table
    db.execute(R"(
        CREATE TABLE users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )");
    
    // Insert data
    db.execute("INSERT INTO users (name, email) VALUES ('John', 'john@example.com')");
    
    // Query data
    db.query("SELECT * FROM users", [](const std::vector<std::string>& row) {
        std::cout << "ID: " << row[0] << ", Name: " << row[1] << std::endl;
        return true;  // continue
    });
    
    db.close();
}
```

## Database Class

### Opening and Closing

```cpp
Database db;

// Open database file (creates if not exists)
db.open("/path/to/database.db");

// Check if open
if (db.isOpen()) {
    std::cout << "Database is open" << std::endl;
}

// Close database
db.close();
```

### Direct SQL Execution

```cpp
// Execute DDL (CREATE, DROP, ALTER)
db.execute("CREATE TABLE IF NOT EXISTS products (id INTEGER PRIMARY KEY, name TEXT)");

// Execute DML (INSERT, UPDATE, DELETE)
db.execute("INSERT INTO products (name) VALUES ('Widget')");
db.execute("UPDATE products SET name = 'Super Widget' WHERE id = 1");
db.execute("DELETE FROM products WHERE id = 2");

// Formatted SQL (using printf-like syntax)
db.executef("INSERT INTO users (name, email) VALUES ('%s', '%s')", name.c_str(), email.c_str());
```

### Querying Data

```cpp
// Query with callback
db.query("SELECT id, name, email FROM users", [](const std::vector<std::string>& columns) {
    std::cout << "ID: " << columns[0] << ", Name: " << columns[1] << std::endl;
    return true;  // Continue iterating
});

// Query single row
auto row = db.queryOne("SELECT * FROM users WHERE id = 1");
if (row) {
    std::cout << "Found user: " << (*row)[1] << std::endl;
}

// Query with no results
db.query("SELECT COUNT(*) FROM users", [](const std::vector<std::string>& columns) {
    std::cout << "Total users: " << columns[0] << std::endl;
    return false;  // Stop after first row
});
```

## Prepared Statements

### Creating Statements

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE email = ?");
```

### Binding Parameters

```cpp
auto stmt = db.prepare("INSERT INTO users (name, email, age) VALUES (?, ?, ?)");

// Bind by index (1-based)
stmt.bind(1, "Alice");
stmt.bind(2, "alice@example.com");
stmt.bind(3, 25);

// Execute
stmt.step();
stmt.reset();  // Reset for reuse

// Bind another row
stmt.bind(1, "Bob");
stmt.bind(2, "bob@example.com");
stmt.bind(3, 30);
stmt.step();
```

### Supported Types

```cpp
// NULL
stmt.bind(1, nullptr);

// Integer
stmt.bind(1, 42);

// 64-bit integer
stmt.bind(1, int64_t(12345678901234));

// Double
stmt.bind(1, 3.14159);

// String
stmt.bind(1, "Hello, World!");
stmt.bind(1, std::string_view("Hello"));

// Binary (BLOB)
std::vector<uint8_t> data = {0x01, 0x02, 0x03};
stmt.bind(1, std::span<const uint8_t>(data));
```

### Reading Results

```cpp
auto stmt = db.prepare("SELECT id, name, email, created_at FROM users");

while (stmt.step()) {
    int id = std::stoi(stmt.columnText(0));
    std::string name = stmt.columnText(1);
    std::string email = stmt.columnText(2);
    
    // Check for NULL
    bool hasCreated = !stmt.columnIsNull(3);
    
    std::cout << id << ": " << name << " <" << email << ">" << std::endl;
}
```

### Column Access Methods

```cpp
stmt.columnText(0);           // std::string
stmt.columnInt64(1);          // int64_t
stmt.columnDouble(2);         // double
stmt.columnBlob(3);           // std::span<const uint8_t>
stmt.columnIsNull(4);         // bool

// Named column access
stmt.columnText("name");      // By column name
```

## Transactions

### Automatic Transaction

```cpp
// All operations in a single transaction
// Automatically commits on success, rolls back on exception
db.transaction([&]() {
    db.execute("INSERT INTO accounts (name) VALUES ('Checking')");
    db.execute("INSERT INTO accounts (name) VALUES ('Savings')");
    
    int id = db.lastInsertRowId();
    
    db.execute("INSERT INTO transactions (account_id, amount) VALUES (?, ?)", id, 1000);
});
// Committed automatically
```

### Manual Transaction Control

```cpp
db.begin();  // Start transaction

try {
    db.execute("INSERT INTO ...");
    db.execute("UPDATE ...");
    
    db.commit();  // Commit changes
} catch (...) {
    db.rollback();  // Rollback on error
}
```

### Checking Transaction State

```cpp
if (db.inTransaction()) {
    std::cout << "In transaction" << std::endl;
}
```

## Database Configuration

### Busy Timeout

```cpp
// Wait up to 5 seconds when database is locked
db.setBusyTimeout(std::chrono::seconds(5));
```

### Foreign Keys

```cpp
// Enable foreign key enforcement
db.enableForeignKeys(true);

// Disable (default for historical compatibility)
db.enableForeignKeys(false);
```

### WAL Mode

```cpp
// Enable Write-Ahead Logging for better concurrency
db.enableWAL(true);

// WAL is faster for reads and writes from multiple connections
```

### Cache Size

```cpp
// Set cache size in pages (default ~2000 pages, ~16MB)
db.setCacheSize(5000);  // ~40MB cache
```

## Utility Methods

```cpp
// Get last inserted row ID
int64_t id = db.lastInsertRowId();

// Get number of rows affected by last INSERT/UPDATE/DELETE
int64_t affected = db.affectedRows();

// Vacuum (reclaim disk space, optimize)
db.vacuum();

// Get raw sqlite3 handle (for advanced use)
sqlite3* handle = db.handle();
```

## Schema Migration

### Defining Migrations

```cpp
std::vector<Migration> migrations = {
    Migration(
        "create_users",
        R"(
            CREATE TABLE users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT UNIQUE NOT NULL
            )
        )",
        "DROP TABLE users"
    ),
    Migration(
        "add_age_column",
        R"(
            ALTER TABLE users ADD COLUMN age INTEGER
        )",
        R"(
            -- SQLite doesn't support DROP COLUMN easily
            -- This is a simplified example
            ALTER TABLE users DROP COLUMN age
        )"
    )
};
```

### Version Format

```cpp
// Version is parsed from migration name: "name_1.2.3" -> v1.2.3
Migration migration(
    "add_users_table_1.0.0",  // Creates version 1.0.0
    "CREATE TABLE ...",
    "DROP TABLE ..."
);
```

### Running Migrations

```cpp
// Run all pending migrations in order
runMigrations(db, migrations);

// Migrations table is created automatically
// Tracks which migrations have been applied
```

### Migration Example

```cpp
#include "OmniNet.h"

using namespace OmniNet::DB;

class AppDatabase {
private:
    Database db;
    
public:
    AppDatabase(const std::string& path) {
        db.open(path);
        migrate();
    }
    
private:
    void migrate() {
        std::vector<Migration> migrations = {
            Migration(
                "create_users_1.0.0",
                R"(
                    CREATE TABLE users (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        name TEXT NOT NULL,
                        email TEXT UNIQUE NOT NULL,
                        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                    )
                )",
                "DROP TABLE users"
            ),
            Migration(
                "add_sessions_1.1.0",
                R"(
                    CREATE TABLE sessions (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        user_id INTEGER NOT NULL,
                        token TEXT UNIQUE NOT NULL,
                        expires_at DATETIME,
                        FOREIGN KEY (user_id) REFERENCES users(id)
                    )
                )",
                "DROP TABLE sessions"
            ),
            Migration(
                "add_last_login_1.2.0",
                R"(
                    ALTER TABLE users ADD COLUMN last_login DATETIME
                )",
                "ALTER TABLE users DROP COLUMN last_login"
            )
        };
        
        runMigrations(db, migrations);
    }
};
```

## Complete CRUD Example

```cpp
#include "OmniNet.h"
#include <optional>

using namespace OmniNet::DB;

class UserRepository {
private:
    Database& db;
    
public:
    explicit UserRepository(Database& database) : db(database) {
        initializeSchema();
    }
    
    struct User {
        int64_t id;
        std::string name;
        std::string email;
        std::optional<std::chrono::system_clock::time_point> lastLogin;
    };
    
    // Create
    int64_t create(const std::string& name, const std::string& email) {
        auto stmt = db.prepare(
            "INSERT INTO users (name, email) VALUES (?, ?)"
        );
        stmt.bind(1, name);
        stmt.bind(2, email);
        stmt.step();
        return db.lastInsertRowId();
    }
    
    // Read all
    std::vector<User> findAll() {
        std::vector<User> users;
        db.query("SELECT id, name, email, last_login FROM users ORDER BY id", 
            [&users](const std::vector<std::string>& row) {
                User user;
                user.id = std::stoll(row[0]);
                user.name = row[1];
                user.email = row[2];
                if (!row[3].empty()) {
                    // Parse datetime
                }
                users.push_back(user);
                return true;
            }
        );
        return users;
    }
    
    // Read one
    std::optional<User> findById(int64_t id) {
        auto stmt = db.prepare(
            "SELECT id, name, email, last_login FROM users WHERE id = ?"
        );
        stmt.bind(1, id);
        
        if (stmt.step()) {
            User user;
            user.id = std::stoll(stmt.columnText(0));
            user.name = stmt.columnText(1);
            user.email = stmt.columnText(2);
            return user;
        }
        return std::nullopt;
    }
    
    // Update
    void update(const User& user) {
        auto stmt = db.prepare(
            "UPDATE users SET name = ?, email = ? WHERE id = ?"
        );
        stmt.bind(1, user.name);
        stmt.bind(2, user.email);
        stmt.bind(3, user.id);
        stmt.step();
    }
    
    // Delete
    void remove(int64_t id) {
        auto stmt = db.prepare("DELETE FROM users WHERE id = ?");
        stmt.bind(1, id);
        stmt.step();
    }
    
private:
    void initializeSchema() {
        db.execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT UNIQUE NOT NULL,
                last_login DATETIME,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )");
    }
};
```

## Error Handling

```cpp
try {
    db.execute("INVALID SQL SYNTAX");
} catch (const DatabaseException& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
    std::cerr << "Error code: " << e.errorCode() << std::endl;
}
```

## Thread Safety

```cpp
// Database class is thread-safe
// Multiple threads can safely use the same Database instance

// For connection pooling, use separate Database instances
Database db1("pooled.db");
Database db2("pooled.db");  // Separate connections
```

## Performance Tips

```cpp
// Use transactions for bulk operations
db.transaction([&]() {
    for (const auto& item : items) {
        db.execute("INSERT INTO ... VALUES (...)", item);
    }
});

// Use prepared statements for repeated queries
auto stmt = db.prepare("SELECT * FROM users WHERE email = ?");
for (const auto& email : emails) {
    stmt.bind(1, email);
    if (stmt.step()) {
        // Process result
    }
    stmt.reset();
}

// Enable WAL for better concurrency
db.enableWAL(true);

// Increase cache size for better read performance
db.setCacheSize(10000);
```

## See Also

- [HTTP Server API](http-server.md) - HTTP server reference
- [WebSocket API](websocket.md) - WebSocket connections
- [Config System](../config.md) - Configuration options