# OmniNet Database API

## Overview

The `OmniNet::DB` namespace provides a thread-safe SQLite3 wrapper with:
- Prepared statements with parameter binding
- Transaction support (manual and automatic)
- Schema migration system
- Thread-safe operations via mutex

## Quick Start

```cpp
#include "OmniNet.h"
#include "db/Database.h"

using namespace OmniNet::DB;

int main() {
    OmniNet::init();
    
    Database db;
    db.open("myapp.db");
    
    // Create table
    db.execute(R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            email TEXT UNIQUE NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )");
    
    // Insert data
    db.execute("INSERT INTO users (name, email) VALUES ('John', 'john@example.com')");
    
    // Query data
    db.query("SELECT id, name, email FROM users", [](const std::vector<std::string>& row) {
        std::cout << "ID: " << row[0] << ", Name: " << row[1] << ", Email: " << row[2] << std::endl;
        return true;  // Continue iterating
    });
    
    db.close();
    OmniNet::cleanup();
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
db.execute("DROP TABLE IF EXISTS old_table");

// Execute DML (INSERT, UPDATE, DELETE)
db.execute("INSERT INTO products (name) VALUES ('Widget')");
db.execute("UPDATE products SET name = 'Super Widget' WHERE id = 1");
db.execute("DELETE FROM products WHERE id = 2");

// Formatted SQL (printf-like)
db.executef("INSERT INTO users (name, email) VALUES ('%s', '%s')", 
    name.c_str(), email.c_str());
```

### Querying Data

```cpp
// Query with callback
db.query("SELECT id, name, email FROM users", 
    [](const std::vector<std::string>& columns) {
        std::cout << "ID: " << columns[0] << ", Name: " << columns[1] << std::endl;
        return true;  // Return true to continue, false to stop
    });

// Query single row
auto row = db.queryOne("SELECT * FROM users WHERE id = 1");
if (row) {
    std::cout << "Found user: " << (*row)[1] << std::endl;
    // (*row)[0] = id
    // (*row)[1] = name
    // (*row)[2] = email
}

// Query with no results
db.query("SELECT COUNT(*) FROM users", 
    [](const std::vector<std::string>& columns) {
        std::cout << "Total users: " << columns[0] << std::endl;
        return false;  // Stop after first row
    });
```

## Prepared Statements

### Creating Statements

```cpp
auto stmt = db.prepare("INSERT INTO users (name, email) VALUES (?, ?)");

// Bind parameters
stmt.bind(1, "Alice");
stmt.bind(2, "alice@example.com");

// Execute
stmt.step();

// Reset for reuse
stmt.reset();

// Bind new values
stmt.bind(1, "Bob");
stmt.bind(2, "bob@example.com");
stmt.step();
```

### Binding Parameters

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE id = ? AND active = ?");

// By index (1-based)
stmt.bind(1, 42);                      // int
stmt.bind(2, 12345678901234LL);         // int64_t
stmt.bind(3, 3.14159);                  // double
stmt.bind(4, "text value");              // std::string
stmt.bind(5, std::string_view("sv"));   // string_view
stmt.bind(6, nullptr);                   // NULL

// By name (using :name syntax)
auto stmt2 = db.prepare("SELECT * FROM users WHERE name = :name");
stmt2.bind(":name", "Alice");
```

### Reading Results

```cpp
auto stmt = db.prepare("SELECT id, name, email, created_at FROM users");
stmt.step();  // Move to first row

// Get column count
int count = stmt.columnCount();

// Get values by index
int id = stmt.columnInt64(0);
std::string name = stmt.columnText(1);
double value = stmt.columnDouble(2);

// Check for NULL
bool isNull = stmt.columnIsNull(3);

// Get blob
auto blob = stmt.columnBlob(4);

// Get by name (if query includes column names)
std::string name = stmt.columnText("name");
```

### Reusing Statements

```cpp
auto stmt = db.prepare("SELECT * FROM users WHERE active = ?");

stmt.bind(1, 1);  // Active users
while (stmt.step()) {
    // Process row
}
stmt.reset();

stmt.bind(1, 0);  // Inactive users
while (stmt.step()) {
    // Process row
}
```

## Transactions

### Automatic Transactions

```cpp
// All operations in fn() run in a transaction
// Automatically commits on success, rolls back on exception
db.transaction([]() {
    db.execute("INSERT INTO users (name) VALUES ('Alice')");
    db.execute("INSERT INTO users (name) VALUES ('Bob')");
    db.execute("UPDATE stats SET count = count + 2");
});  // Commits here

// If exception thrown, automatically rolled back
try {
    db.transaction([]() {
        db.execute("INSERT INTO users (name) VALUES ('Charlie')");
        db.execute("INSERT INTO users (name) VALUES ('Dave')");
        throw std::runtime_error("Something went wrong");
    });
} catch (...) {
    // Changes rolled back, no users inserted
}
```

### Manual Transactions

```cpp
db.begin();  // Start transaction

try {
    db.execute("INSERT INTO users (name) VALUES ('Alice')");
    db.execute("INSERT INTO users (name) VALUES ('Bob')");
    
    if (someCondition) {
        db.commit();  // Save changes
    } else {
        db.rollback();  // Undo changes
    }
} catch (...) {
    db.rollback();  // Clean up on error
}

// Check transaction state
if (db.inTransaction()) {
    std::cout << "Still in transaction" << std::endl;
}
```

## Schema Migrations

### Defining Migrations

```cpp
Migration migration(
    "add_users_table",   // Name
    R"(  // UP - apply migration
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
        )
    )",
    R"(  // DOWN - revert migration
        DROP TABLE IF EXISTS users
    )"
);

// Version is extracted from migration name if it follows pattern: "v1_2_3_description"
```

### Running Migrations

```cpp
std::vector<Migration> migrations = {
    Migration("v1_0_0_initial", ...),
    Migration("v1_0_1_add_email", ...),
    Migration("v1_1_0_add_roles", ...)
};

runMigrations(db, migrations);
```

## Configuration

### Busy Timeout

Set how long to wait for locks:

```cpp
db.setBusyTimeout(std::chrono::seconds(5));
```

### Foreign Keys

```cpp
db.enableForeignKeys(true);  // Enforce foreign key constraints
```

### WAL Mode

```cpp
db.enableWAL(true);  // Better concurrency for reads
```

### Cache Size

```cpp
db.setCacheSize(10000);  // Number of pages
```

## Utility Methods

### Last Inserted Row

```cpp
db.execute("INSERT INTO users (name) VALUES ('Alice')");
int64_t id = db.lastInsertRowId();
```

### Affected Rows

```cpp
db.execute("DELETE FROM users WHERE active = 0");
int64_t count = db.affectedRows();
```

### Vacuum

```cpp
db.vacuum();  // Reclaim unused space
```

## Thread Safety

All Database methods are thread-safe:

```cpp
// Safe to use from multiple threads
std::thread t1([&db]() {
    db.query("SELECT * FROM users", ...);
});

std::thread t2([&db]() {
    db.execute("INSERT INTO users ...");
});

t1.join();
t2.join();
```

## Error Handling

```cpp
try {
    db.execute("INSERT INTO users (email) VALUES ('existing@example.com')");
} catch (const DatabaseException& e) {
    std::cerr << "Database error: " << e.what() << std::endl;
    // Error codes available via errorCode()
}
```

## Example: User Management

```cpp
class UserStore {
public:
    UserStore(Database& db) : m_db(db) {}
    
    void createTable() {
        m_db.execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT UNIQUE NOT NULL,
                email TEXT UNIQUE NOT NULL,
                password_hash TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        )");
    }
    
    int64_t createUser(const std::string& username, 
                       const std::string& email,
                       const std::string& passwordHash) {
        m_db.executef(
            "INSERT INTO users (username, email, password_hash) VALUES ('%s', '%s', '%s')",
            username.c_str(), email.c_str(), passwordHash.c_str()
        );
        return m_db.lastInsertRowId();
    }
    
    std::optional<std::vector<std::string>> findByUsername(const std::string& username) {
        auto stmt = m_db.prepare("SELECT * FROM users WHERE username = ?");
        stmt.bind(1, username);
        
        if (stmt.step()) {
            std::vector<std::string> row;
            for (int i = 0; i < stmt.columnCount(); i++) {
                row.push_back(stmt.columnText(i));
            }
            return row;
        }
        return std::nullopt;
    }
    
    bool deleteUser(int64_t id) {
        m_db.execute("DELETE FROM users WHERE id = " + std::to_string(id));
        return m_db.affectedRows() > 0;
    }
    
private:
    Database& m_db;
};

// Usage
Database db;
db.open("app.db");

UserStore users(db);
users.createTable();

int64_t id = users.createUser("alice", "alice@example.com", "hash123");
auto user = users.findByUsername("alice");
users.deleteUser(id);
```

## Example: Blog with Transactions

```cpp
void createPost(Database& db, const std::string& title, 
                const std::string& content, const std::string& author) {
    db.transaction([&]() {
        // Insert post
        db.executef("INSERT INTO posts (title, content, author) VALUES ('%s', '%s', '%s')",
            title.c_str(), content.c_str(), author.c_str());
        int64_t postId = db.lastInsertRowId();
        
        // Update author's post count
        db.executef("UPDATE users SET post_count = post_count + 1 WHERE username = '%s'",
            author.c_str());
        
        // Log the action
        db.executef("INSERT INTO audit_log (action, target_id) VALUES ('create_post', %ld)",
            postId);
    });
}

void deletePost(Database& db, int64_t postId) {
    db.transaction([&]() {
        // Get author before deletion
        auto row = db.queryOne("SELECT author FROM posts WHERE id = " + std::to_string(postId));
        if (row) {
            std::string author = (*row)[0];
            
            // Delete post
            db.execute("DELETE FROM posts WHERE id = " + std::to_string(postId));
            
            // Update author's post count
            db.executef("UPDATE users SET post_count = post_count - 1 WHERE username = '%s'",
                author.c_str());
        }
    });
}
```