// OmniNet - Vector Store (sqlite-vec wrapper)
#ifndef OMNINET_VECTOR_STORE_H
#define OMNINET_VECTOR_STORE_H

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <limits>

#include "../db/Database.h"

namespace OmniNet::Vector {

// ============================================================================
// Search Result
// ============================================================================

struct SearchResult {
    int64_t rowid;
    float distance;
    std::string metadata;
};

// ============================================================================
// Vector Store
// ============================================================================

class VectorStore {
public:
    // Constructor - takes existing database reference
    explicit VectorStore(DB::Database& db);
    
    // Destructor
    ~VectorStore();
    
    // Non-copyable (contains reference)
    VectorStore(const VectorStore&) = delete;
    VectorStore& operator=(const VectorStore&) = delete;
    
    // Move constructor only (move assignment deleted due to reference member)
    VectorStore(VectorStore&& other) noexcept;
    VectorStore& operator=(VectorStore&&) = delete;
    
    // --- Index Management ---
    
    // Create a new vector index (virtual table)
    // dimensions: embedding vector size (e.g., 768, 1536)
    // metric: "cosine" (default), "l2", "hamming"
    void createIndex(const std::string& name, size_t dimensions, 
                     const std::string& metric = "cosine");
    
    // Check if index exists
    bool indexExists(const std::string& name) const;
    
    // Drop/delete an index
    void dropIndex(const std::string& name);
    
    // List all vector indices
    std::vector<std::string> listIndices() const;
    
    // --- CRUD Operations ---
    
    // Insert a single embedding
    // Returns the rowid of the inserted record
    int64_t insert(const std::string& indexName,
                    const std::vector<float>& embedding,
                    const std::string& metadata = "{}");
    
    // Insert with explicit rowid
    void insert(int64_t rowid,
                const std::string& indexName,
                const std::vector<float>& embedding,
                const std::string& metadata = "{}");
    
    // Batch insert embeddings
    // Returns vector of rowids for inserted records
    std::vector<int64_t> insertBatch(const std::string& indexName,
                                      const std::vector<std::vector<float>>& embeddings,
                                      const std::vector<std::string>& metadata = {});
    
    // Delete by rowid
    void remove(const std::string& indexName, int64_t rowid);
    
    // Delete multiple by rowids
    void removeBatch(const std::string& indexName, const std::vector<int64_t>& rowids);
    
    // --- Search Operations ---
    
    // Search for nearest neighbors
    // k: number of results to return
    // radius: maximum distance (optional filter)
    std::vector<SearchResult> search(const std::string& indexName,
                                     const std::vector<float>& queryEmbedding,
                                     size_t k = 5,
                                     float radius = std::numeric_limits<float>::max());
    
    // --- Statistics ---
    
    // Get count of vectors in index
    size_t count(const std::string& indexName) const;
    
    // Get index info (dimensions, metric)
    struct IndexInfo {
        std::string name;
        size_t dimensions;
        std::string metric;
        size_t count;
    };
    std::optional<IndexInfo> getIndexInfo(const std::string& name) const;
    
    // --- Extension Loading ---
    
    // Load sqlite-vec extension (called automatically by constructor)
    // path: path to vec0.so, empty uses default search path
    void loadExtension(const std::string& path = {});
    
    // Check if extension is loaded
    bool extensionLoaded() const;

private:
    DB::Database& m_db;
    bool m_extensionLoaded = false;
    
    // Helper to format vector as JSON string for sqlite-vec
    static std::string formatEmbedding(const std::vector<float>& embedding);
    
    // Helper to parse metadata JSON
    static std::string sanitizeMetadata(const std::string& metadata);
};

// ============================================================================
// Exception
// ============================================================================

class VectorStoreException : public std::runtime_error {
public:
    VectorStoreException(const std::string& message) 
        : std::runtime_error(message) {}
};

} // namespace OmniNet::Vector

#endif // OMNINET_VECTOR_STORE_H
