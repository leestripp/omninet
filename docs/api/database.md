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

---

# Vector Database (sqlite-vec)

## Overview

The `OmniNet::Vector` namespace provides vector storage and similarity search using **sqlite-vec**, an SQLite extension for vector search. It wraps the sqlite-vec virtual tables with a clean C++ API.

**Important Design Decision**: OmniNet handles **storage and search** of vectors. **Embedding generation** is the responsibility of your application. Use any embedding provider (Ollama, OpenAI, local models, etc.) to generate vectors, then store and search them with OmniNet.

## Quick Start

```cpp
#include "db/Database.h"
#include "vector/VectorStore.h"

// Your app generates embeddings with Ollama, OpenAI, etc.
std::vector<float> embedding = yourEmbedder.embed("Your text here");

// Store in OmniNet
Database db;
db.open("vectors.db");
db.execute("SELECT load_extension('/path/to/vec0.so')");  // Load sqlite-vec

VectorStore store(db);

// Create index
store.createIndex("documents", 768, "cosine");

// Insert
store.insert("documents", embedding, R"({"title": "My Document"})");

// Search
auto results = store.search("documents", queryEmbedding, k=5);
for (const auto& r : results) {
    std::cout << "Row: " << r.rowid << ", Distance: " << r.distance << std::endl;
}
```

## Extension Loading

Before using VectorStore, you must load the `vec0.so` sqlite-vec extension:

```cpp
// Load the extension (once per database connection)
db.execute("SELECT load_extension('/path/to/vec0.so')");

// Or let VectorStore try to load it automatically
VectorStore store(db);  // Will attempt to load default vec0.so
```

**Build sqlite-vec**:
```bash
cd third_party/sqlite-vec
# Create header from template
sed -e 's/\${VERSION}/0.1.10/' -e 's/\${VERSION_MAJOR}/0/' \
    -e 's/\${VERSION_MINOR}/1/' -e 's/\${VERSION_PATCH}/10/' \
    sqlite-vec.h.tmpl > sqlite-vec.h
# Compile
gcc -shared -fPIC -O3 sqlite-vec.c -o vec0.so -I/usr/include -I. -lm
```

## Index Management

### Create Index

```cpp
// Create a vector index
// dimensions: embedding vector size (e.g., 768 for nomic-embed-text)
// metric: "cosine" (default), "l2", or "hamming"
store.createIndex("documents", 768, "cosine");

// Create with L2 (Euclidean) distance
store.createIndex("vectors_l2", 1536, "l2");
```

### Check Index Exists

```cpp
if (store.indexExists("documents")) {
    std::cout << "Index exists" << std::endl;
}
```

### List All Indices

```cpp
auto indices = store.listIndices();
for (const auto& name : indices) {
    std::cout << "  " << name << std::endl;
}
```

### Drop Index

```cpp
store.dropIndex("old_index");
```

## Inserting Vectors

### Single Insert

```cpp
// Your embedding vector (e.g., from Ollama/OpenAI)
std::vector<float> embedding = {0.123, -0.456, 0.789, /* ... 768 floats */};

// Insert with metadata (JSON)
store.insert("documents", embedding, R"({
    "title": "Introduction to AI",
    "category": "technology",
    "source": "user123"
})");

// Returns the rowid of inserted record
int64_t id = store.insert("documents", embedding, "{}");
```

### Batch Insert

```cpp
std::vector<std::vector<float>> embeddings = {
    embedding1,
    embedding2,
    embedding3
};

std::vector<std::string> metadata = {
    R"({"title": "Doc 1"})",
    R"({"title": "Doc 2"})",
    R"({"title": "Doc 3"})"
};

// Insert batch
auto rowids = store.insertBatch("documents", embeddings, metadata);
std::cout << "Inserted " << rowids.size() << " vectors" << std::endl;
```

### Insert with Explicit Rowid

```cpp
store.insert(42, "documents", embedding, R"({"id": 42})");
```

## Searching

### Nearest Neighbor Search

```cpp
// Search for k nearest neighbors
std::vector<float> queryEmbedding = yourEmbedder.embed("search query");

auto results = store.search("documents", queryEmbedding, k=5);

for (const auto& result : results) {
    std::cout << "Row: " << result.rowid << std::endl;
    std::cout << "Distance: " << result.distance << std::endl;
    std::cout << "Metadata: " << result.metadata << std::endl;
}
```

### Search with Radius Filter

```cpp
// Only return results within distance radius
auto results = store.search("documents", queryEmbedding, k=10, radius=1.5f);
```

### SearchResult Structure

```cpp
struct SearchResult {
    int64_t rowid;        // Row ID in the vector table
    float distance;        // Distance from query vector
    std::string metadata; // JSON metadata string
};
```

## Deleting Vectors

### Single Delete

```cpp
store.remove("documents", 42);
```

### Batch Delete

```cpp
store.removeBatch("documents", {1, 2, 3, 4, 5});
```

## Statistics

### Count Vectors

```cpp
size_t count = store.count("documents");
std::cout << "Total vectors: " << count << std::endl;
```

### Get Index Info

```cpp
auto info = store.getIndexInfo("documents");
if (info) {
    std::cout << "Name: " << info->name << std::endl;
    std::cout << "Dimensions: " << info->dimensions << std::endl;
    std::cout << "Count: " << info->count << std::endl;
}
```

## Extension Status

### Check if Loaded

```cpp
VectorStore store(db);
if (store.extensionLoaded()) {
    std::cout << "vec0 extension is loaded" << std::endl;
} else {
    std::cout << "Extension not loaded" << std::endl;
}
```

### Manual Load

```cpp
store.loadExtension("/path/to/vec0.so");
```

## Complete Example

```cpp
#include "db/Database.h"
#include "vector/VectorStore.h"
#include <iostream>

int main() {
    // Open database
    Database db;
    db.open("vectors.db");
    
    // Load sqlite-vec extension
    db.execute("SELECT load_extension('./vec0.so')");
    
    // Create vector store
    VectorStore store(db);
    
    // Create index for 768-dimensional embeddings
    store.createIndex("articles", 768, "cosine");
    
    // Simulate embedding generation (your app does this)
    std::vector<float> emb1 = { /* 768 floats */ };
    std::vector<float> emb2 = { /* 768 floats */ };
    
    // Insert vectors with metadata
    store.insert("articles", emb1, R"({
        "title": "Understanding Neural Networks",
        "tags": ["AI", "ML", "Tutorial"]
    })");
    
    store.insert("articles", emb2, R"({
        "title": "Introduction to Vector Databases",
        "tags": ["Database", "Vectors"]
    })");
    
    // Search
    std::vector<float> query = yourEmbedder.embed("machine learning basics");
    auto results = store.search("articles", query, k=3);
    
    std::cout << "Found " << results.size() << " similar articles" << std::endl;
    for (const auto& r : results) {
        std::cout << "  ID: " << r.rowid 
                  << ", Distance: " << r.distance << std::endl;
    }
    
    return 0;
}
```

## HTTP Integration

To add vector endpoints to your HTTP server:

```cpp
// POST /api/vector/create - Create index
router.post("/api/vector/create", [](Request& req, Response& resp) {
    auto body = json::parse(req.body());
    std::string name = body["name"];
    size_t dims = body["dimensions"];
    std::string metric = body.value("metric", "cosine");
    
    store.createIndex(name, dims, metric);
    resp.json({{"status", "created"}});
});

// POST /api/vector/insert - Insert embedding
router.post("/api/vector/insert", [](Request& req, Response& resp) {
    auto body = json::parse(req.body());
    auto embedding = body["embedding"].get<std::vector<float>>();
    std::string meta = body.value("metadata", "{}");
    
    int64_t id = store.insert(body["index"], embedding, meta);
    resp.json({{"id", id}});
});

// POST /api/vector/search - Search
router.post("/api/vector/search", [](Request& req, Response& resp) {
    auto body = json::parse(req.body());
    auto query = body["embedding"].get<std::vector<float>>();
    size_t k = body.value("k", 5);
    
    auto results = store.search(body["index"], query, k);
    
    json::array_t items;
    for (const auto& r : results) {
        items.push_back({
            {"rowid", r.rowid},
            {"distance", r.distance},
            {"metadata", json::parse(r.metadata)}
        });
    }
    resp.json({{"results", items}});
});
```

## Error Handling

```cpp
try {
    store.createIndex("documents", 768, "invalid_metric");
} catch (const VectorStoreException& e) {
    std::cerr << "VectorStore error: " << e.what() << std::endl;
}

try {
    store.insert("nonexistent", embedding);
} catch (const VectorStoreException& e) {
    std::cerr << "Index not found: " << e.what() << std::endl;
}
```
```