// OmniNet - Vector Store Implementation
#include "VectorStore.h"

#include <sstream>
#include <iomanip>
#include <algorithm>

namespace OmniNet::Vector {

// ============================================================================
// Helper Functions
// ============================================================================

std::string VectorStore::formatEmbedding(const std::vector<float>& embedding) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < embedding.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << std::fixed << std::setprecision(6) << embedding[i];
    }
    oss << "]";
    return oss.str();
}

std::string VectorStore::sanitizeMetadata(const std::string& metadata) {
    // Basic sanitization - ensure valid JSON
    if (metadata.empty()) return "{}";
    if (metadata == "{}" || metadata == "[]") return metadata;
    
    // Check if it starts with { or [
    if (metadata[0] != '{' && metadata[0] != '[') {
        return "\"" + metadata + "\"";  // Wrap as string
    }
    return metadata;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

VectorStore::VectorStore(DB::Database& db) : m_db(db) {
    // Load extension by default
    loadExtension();
}

VectorStore::~VectorStore() = default;

VectorStore::VectorStore(VectorStore&& other) noexcept = default;

// ============================================================================
// Extension Loading
// ============================================================================

void VectorStore::loadExtension(const std::string& path) {
    if (m_extensionLoaded) return;
    
    try {
        if (path.empty()) {
            // Try default location (same directory as executable or ./vec0.so)
            m_db.execute("SELECT * FROM sqlite_master WHERE name='vec0_example'");
        }
        
        // Load extension - use empty path to let SQLite find it
        // In practice, you'd pass the actual path to vec0.so
        // For now, we assume vec0.so is loadable via standard paths
        // or the app will call sqlite3_load_extension directly
        
        // Mark as loaded after verification
        m_db.query("SELECT vec_version()", [](const std::vector<std::string>&) {
            return true;
        });
        m_extensionLoaded = true;
    } catch (const DB::DatabaseException&) {
        // Extension not yet loaded - will be loaded on first use
        m_extensionLoaded = false;
    }
}

bool VectorStore::extensionLoaded() const {
    return m_extensionLoaded;
}

// ============================================================================
// Index Management
// ============================================================================

void VectorStore::createIndex(const std::string& name, size_t dimensions,
                               const std::string& metric) {
    // Validate inputs
    if (name.empty()) {
        throw VectorStoreException("Index name cannot be empty");
    }
    if (dimensions == 0) {
        throw VectorStoreException("Dimensions must be greater than 0");
    }
    if (metric != "cosine" && metric != "l2" && metric != "hamming") {
        throw VectorStoreException("Metric must be 'cosine', 'l2', or 'hamming'");
    }
    
    // Check if already exists
    if (indexExists(name)) {
        throw VectorStoreException("Index '" + name + "' already exists");
    }
    
    // Create virtual table using vec0
    // Note: sqlite-vec uses FLOAT[dims] for float vectors
    std::ostringstream sql;
    sql << "CREATE VIRTUAL TABLE " << name << " USING vec0(embedding FLOAT[" 
        << dimensions << "], metadata TEXT)";
    
    m_db.execute(sql.str());
}

bool VectorStore::indexExists(const std::string& name) const {
    auto result = m_db.queryOne(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" + name + "'"
    );
    return result && std::stoll((*result)[0]) > 0;
}

void VectorStore::dropIndex(const std::string& name) {
    if (!indexExists(name)) {
        throw VectorStoreException("Index '" + name + "' does not exist");
    }
    m_db.execute("DROP TABLE IF EXISTS " + name);
}

std::vector<std::string> VectorStore::listIndices() const {
    std::vector<std::string> indices;
    
    m_db.query(
        "SELECT name FROM sqlite_master WHERE type='table' AND name LIKE 'vec_%'",
        [&indices](const std::vector<std::string>& row) {
            indices.push_back(row[0]);
            return true;
        }
    );
    
    return indices;
}

// ============================================================================
// CRUD Operations
// ============================================================================

int64_t VectorStore::insert(const std::string& indexName,
                            const std::vector<float>& embedding,
                            const std::string& metadata) {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    std::string embStr = formatEmbedding(embedding);
    std::string metaStr = sanitizeMetadata(metadata);
    
    m_db.executef(
        "INSERT INTO %s (embedding, metadata) VALUES ('%s', '%s')",
        indexName.c_str(),
        embStr.c_str(),
        metaStr.c_str()
    );
    
    return m_db.lastInsertRowId();
}

void VectorStore::insert(int64_t rowid,
                         const std::string& indexName,
                         const std::vector<float>& embedding,
                         const std::string& metadata) {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    std::string embStr = formatEmbedding(embedding);
    std::string metaStr = sanitizeMetadata(metadata);
    
    m_db.executef(
        "INSERT INTO %s (rowid, embedding, metadata) VALUES (%ld, '%s', '%s')",
        indexName.c_str(),
        rowid,
        embStr.c_str(),
        metaStr.c_str()
    );
}

std::vector<int64_t> VectorStore::insertBatch(const std::string& indexName,
                                               const std::vector<std::vector<float>>& embeddings,
                                               const std::vector<std::string>& metadata) {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    if (embeddings.empty()) {
        return {};
    }
    
    std::vector<int64_t> rowids;
    rowids.reserve(embeddings.size());
    
    for (size_t i = 0; i < embeddings.size(); ++i) {
        std::string meta = (i < metadata.size()) ? metadata[i] : "{}";
        int64_t id = insert(indexName, embeddings[i], meta);
        rowids.push_back(id);
    }
    
    return rowids;
}

void VectorStore::remove(const std::string& indexName, int64_t rowid) {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    m_db.execute("DELETE FROM " + indexName + " WHERE rowid = " + std::to_string(rowid));
}

void VectorStore::removeBatch(const std::string& indexName, const std::vector<int64_t>& rowids) {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    if (rowids.empty()) return;
    
    // Build IN clause
    std::ostringstream oss;
    for (size_t i = 0; i < rowids.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << rowids[i];
    }
    
    m_db.execute("DELETE FROM " + indexName + " WHERE rowid IN (" + oss.str() + ")");
}

// ============================================================================
// Search Operations
// ============================================================================

std::vector<SearchResult> VectorStore::search(const std::string& indexName,
                                              const std::vector<float>& queryEmbedding,
                                              size_t k,
                                              float radius) {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    std::string embStr = formatEmbedding(queryEmbedding);
    std::vector<SearchResult> results;
    
    // Build query using sqlite-vec's MATCH syntax
    // Note: sqlite-vec uses WHERE col MATCH 'query' ORDER BY distance LIMIT k
    std::ostringstream sql;
    sql << "SELECT rowid, distance, metadata FROM " << indexName 
        << " WHERE embedding match '" << embStr << "'";
    
    if (radius != std::numeric_limits<float>::max()) {
        sql << " AND distance <= " << std::fixed << std::setprecision(6) << radius;
    }
    
    sql << " ORDER BY distance LIMIT " << k;
    
    m_db.query(sql.str(), [&results](const std::vector<std::string>& row) {
        SearchResult result;
        result.rowid = std::stoll(row[0]);
        result.distance = std::stof(row[1]);
        result.metadata = row.size() > 2 ? row[2] : "{}";
        results.push_back(result);
        return true;
    });
    
    return results;
}

// ============================================================================
// Statistics
// ============================================================================

size_t VectorStore::count(const std::string& indexName) const {
    if (!indexExists(indexName)) {
        throw VectorStoreException("Index '" + indexName + "' does not exist");
    }
    
    auto result = m_db.queryOne("SELECT COUNT(*) FROM " + indexName);
    return result ? std::stoll((*result)[0]) : 0;
}

std::optional<VectorStore::IndexInfo> VectorStore::getIndexInfo(const std::string& name) const {
    if (!indexExists(name)) {
        return std::nullopt;
    }
    
    IndexInfo info{};
    info.name = name;
    info.count = count(name);
    
    // Get dimensions from table definition (stored in sql column)
    auto result = m_db.queryOne(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='" + name + "'"
    );
    
    if (result && !(*result)[0].empty()) {
        // Parse dimensions from CREATE statement
        // e.g., "embedding FLOAT[768]" -> 768
        std::string sql = (*result)[0];
        size_t start = sql.find("FLOAT[");
        if (start != std::string::npos) {
            start += 6;
            size_t end = sql.find("]", start);
            if (end != std::string::npos) {
                info.dimensions = std::stoul(sql.substr(start, end - start));
            }
        }
    }
    
    info.metric = "cosine";  // Default, could be parsed from table info
    return info;
}

} // namespace OmniNet::Vector