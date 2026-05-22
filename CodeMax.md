# OmniNet - Code Analysis and Development Plan

## System Information
| Component | Value |
|-----------|-------|
| Kernel | Linux 7.0.3-6.stable |
| OS | Ubuntu 26.04 LTS (Resolute Raccoon) |
| Container | podman (ubuntu:26.04) |
| Display | Wayland, GNOME |
| Architecture | x86_64 |
| Session | Local, xterm-256color |
| User | lee |
| Hostname | dev-pc |

## Project Info
- **Type**: C++ HTTP Server Library (C++20)
- **Build**: CMake
- **Dependencies**: OpenSSL, SQLite3, nlohmann/json, pthread, zlib (optional)

---

## SQLite Support Analysis

### Overview
OmniNet has comprehensive SQLite support integrated throughout the codebase via two parallel implementations:

1. **Library Implementation** (`src/db/`) - Thread-safe database wrapper for runtime use
2. **Tools Implementation** (`tools/tools_common/`) - Simplified database access for CLI tools

### 1. Library Database Layer (`src/db/`)

#### Core Files
| File | Purpose |
|------|---------|
| `Database.h` | SQLite3 wrapper with thread-safe operations, prepared statements, transactions |
| `Database.cpp` | Implementation of database wrapper |
| `SessionStore.h/cpp` | Session management with TTL, cleanup, middleware support |
| `UserStore.h/cpp` | User authentication, RBAC, Basic Auth support |

#### Key Features

**Database Class (`OmniNet::DB::Database`)**:
- Thread-safe via `std::mutex`
- Open/close lifecycle management
- Direct SQL execution with formatted SQL (`executef`)
- Query with callback and single-row queries
- Range-based for loop support via `QueryResult`
- Prepared statements with parameter binding
- Transactions (automatic and manual)
- WAL mode, foreign keys, cache size configuration
- Migration system with version tracking

#### CMake Integration
```cmake
find_package(SQLite3 REQUIRED)
target_link_libraries(OmniNet PUBLIC SQLite::SQLite3)
```

**SQLite Version**: 3.46.1

---

### 2. Tools Database Layer (`tools/tools_common/`)

- `database.h/cpp` - Simplified database class
- `IntrusionStore` - Blocked IPs and intrusion logs
- `UserStore` - User/session management
- `LogReader` - Log file parsing

---

### 3. CLI Database Tool (`tools/omninet-db/`)

Commands: info, tables, schema, query, vacuum, analyze, backup, restore

---

### 4. Known Issues

1. **SQL Injection via Session/User IDs** - String concatenation instead of prepared statements
2. **Tools vs Library Duplication** - Two separate database implementations

---

## Build System

### Build Commands
```bash
# Configure & Build
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j$(nproc)

# Build sqlite-vec extension
cd third_party/sqlite-vec
sed -e 's/\${VERSION}/0.1.10/' -e 's/\${VERSION_MAJOR}/0/' \
    -e 's/\${VERSION_MINOR}/1/' -e 's/\${VERSION_PATCH}/10/' \
    sqlite-vec.h.tmpl > sqlite-vec.h
gcc -shared -fPIC -O3 sqlite-vec.c -o ../../vec0.so \
    -I/usr/include -I. -lm
cd ../..

# Test
ctest --test-dir build --output-on-failure
```

---

## Vector Database Support (sqlite-vec)

**Date**: 2026-05-22
**Status**: Planning Phase

### Design Decision: Embeddings Out of Scope

**Embedding generation is the responsibility of the application using OmniNet**, not OmniNet itself.

**Rationale:**
- **Separation of Concerns** - OmniNet is an HTTP server, not an ML framework
- **Simplicity** - Avoids linking llama.cpp (~50MB, complex build)
- **Flexibility** - App can use Ollama, OpenAI, local models, etc.
- **Performance** - Embedding doesn't block HTTP threads

---

### Architecture

```
┌────────────────────────────────────────────────────────┐
│                    Your Application                    │
├────────────────────────────────────────────────────────┤
│  ┌──────────────┐    ┌──────────────────────────────┐ │
│  │  Embedder    │───▶│        HTTP Client             │ │
│  │ (Ollama,     │    │  POST /api/vector/insert     │ │
│  │  OpenAI...)  │    └──────────────────────────────┘ │
└──┴──────────────┴─────────────────────▼─────────────────┘
                                       │
                         ┌─────────────▼─────────────┐
                         │   OmniNet::VectorStore    │
                         │                           │
                         │  • Store embeddings       │
                         │  • ANN search (HNSW)     │
                         │  • Metadata filtering     │
                         └───────────────────────────┘
```

---

### Components

| Component | Purpose | Technology |
|-----------|---------|------------|
| `VectorStore` | Vector storage & ANN search | sqlite-vec virtual tables |

### Proposed Directory Structure

```
src/vector/
    VectorStore.h/cpp        # Vector storage wrapper
tests/
    test_vector_store.cpp    # Tests
```

---

### sqlite-vec Extension

**Build:** `vec0.so` (212KB) - Already compiled

**Usage:**
```sql
-- Create vector table
CREATE VIRTUAL TABLE documents USING vec0(
    embedding FLOAT[768],
    metadata TEXT
);

-- Insert pre-computed embedding
INSERT INTO documents (embedding, metadata) VALUES (
    '[0.123, -0.456, 0.789, ...]',
    '{"title": "My Doc"}'
);

-- Search
SELECT rowid, distance, metadata
FROM documents
WHERE embedding match '[0.1, -0.4, 0.7, ...]'
ORDER BY distance
LIMIT 5;
```

---

## Implementation Plan

### Phase 1: VectorStore Class

```cpp
namespace OmniNet::Vector {

struct SearchResult {
    int64_t rowid;
    float distance;
    std::string metadata;
};

class VectorStore {
public:
    VectorStore(DB::Database& db);
    
    void createIndex(const std::string& name, size_t dimensions);
    int64_t insert(const std::string& indexName,
                  const std::vector<float>& embedding,
                  const std::string& metadata = "{}");
    std::vector<int64_t> insertBatch(...);
    std::vector<SearchResult> search(const std::string& indexName,
                                     const std::vector<float>& query,
                                     size_t k = 5);
    void remove(int64_t rowid);
    size_t count(const std::string& indexName) const;
    std::vector<std::string> listIndices() const;
};

}
```

### Phase 2: HTTP Endpoints

```cpp
router.post("/api/vector/create", ...);
router.post("/api/vector/insert", ...);
router.post("/api/vector/search", ...);
router.post("/api/vector/delete", ...);
```

---

### Testing Plan

| Test | Purpose |
|------|---------|
| `test_vector_create` | Create vector index |
| `test_vector_insert` | Insert embeddings |
| `test_vector_search` | ANN search |
| `test_vector_delete` | Delete vectors |

---

### Estimated Timeline

| Phase | Tasks | Effort |
|-------|-------|--------|
| **Phase 1** | VectorStore class | 1-2 days |
| **Phase 2** | HTTP endpoints | 1 day |
| **Phase 3** | Testing & docs | 1 day |

**Total**: ~3-4 days

---

## Tasks

### SQLite Security Fixes
- [ ] Fix SQL injection risks in SessionStore/UserStore
- [ ] Consolidate tools/library database implementations

### Vector Database (sqlite-vec only)
- [x] Create `src/vector/` directory
- [x] Implement `VectorStore` class (`src/vector/VectorStore.h/cpp`)
- [x] Add VectorStore to CMakeLists.txt
- [x] Build passes (100%)
- [ ] Add HTTP endpoints to router
- [ ] Write unit tests (basic tests passed manually)

---

## Files Summary

### Source Files (SQLite-related)
| File | Purpose |
|------|---------|
| `src/db/Database.h/cpp` | Main database wrapper |
| `src/db/SessionStore.h/cpp` | Session management |
| `src/db/UserStore.h/cpp` | User management |

### Third-Party Dependencies
| Directory | Purpose |
|-----------|---------|
| `third_party/sqlite-vec/` | Vector storage (sqlite-vec) |

### Documentation
| File | Purpose |
|------|---------|
| `docs/api/database.md` | Database API docs |
| `docs/api/index.md` | API index |
| `docs/api/vector.md` | Vector API (to create) |

---

## Recent Fixes
- **2026-05-22**: Simplified vector plan - embeddings out of scope, sqlite-vec only
- **2026-05-22**: Built sqlite-vec extension (`vec0.so`)
- **2026-05-15**: Fixed SQLite CMake target name

---

## Current Status
- ✅ Build passes
- ✅ Core SQLite functionality working
- ⚠️ SQL injection vulnerabilities in SessionStore/UserStore
- 📋 Vector database support planned (sqlite-vec only)