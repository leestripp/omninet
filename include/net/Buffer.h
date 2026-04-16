// OmniNet - Byte Buffer for Network I/O
#ifndef OMNINET_BUFFER_H
#define OMNINET_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace OmniNet {

// ============================================================================
// Buffer Exceptions
// ============================================================================

class BufferException : public std::runtime_error {
public:
    explicit BufferException(const char* message) : std::runtime_error(message) {}
    explicit BufferException(const std::string& message) : std::runtime_error(message) {}
};

// ============================================================================
// ByteBuffer - Dynamic buffer for network I/O
// ============================================================================

class ByteBuffer {
public:
    // Minimum buffer capacity (16KB)
    static constexpr size_t MIN_CAPACITY = 16 * 1024;
    // Default initial capacity (64KB)
    static constexpr size_t DEFAULT_CAPACITY = 64 * 1024;
    // Maximum capacity (16MB)
    static constexpr size_t MAX_CAPACITY = 16 * 1024 * 1024;
    
    // Constructors
    ByteBuffer() : ByteBuffer(DEFAULT_CAPACITY) {}
    
    explicit ByteBuffer(size_t initialCapacity) {
        if (initialCapacity < MIN_CAPACITY) {
            initialCapacity = MIN_CAPACITY;
        }
        m_capacity = initialCapacity;
        m_data.resize(m_capacity);
    }
    
    // Pre-fill constructor
    ByteBuffer(const void* data, size_t size) : ByteBuffer(std::max(size, MIN_CAPACITY)) {
        if (data && size > 0) {
            std::memcpy(m_data.data(), data, size);
            m_size = size;
        }
    }
    
    // Copy constructor
    ByteBuffer(const ByteBuffer& other) 
        : m_data(other.m_data)
        , m_size(other.m_size)
        , m_capacity(other.m_capacity) {}
    
    // Move constructor
    ByteBuffer(ByteBuffer&& other) noexcept
        : m_data(std::move(other.m_data))
        , m_size(other.m_size)
        , m_capacity(other.m_capacity) {
        other.m_size = 0;
        other.m_capacity = MIN_CAPACITY;
    }
    
    // Copy assignment
    ByteBuffer& operator=(const ByteBuffer& other) {
        if (this != &other) {
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
        }
        return *this;
    }
    
    // Move assignment
    ByteBuffer& operator=(ByteBuffer&& other) noexcept {
        if (this != &other) {
            m_data = std::move(other.m_data);
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_size = 0;
            other.m_capacity = MIN_CAPACITY;
        }
        return *this;
    }
    
    // =========================================================================
    // Capacity Management
    // =========================================================================
    
    size_t capacity() const { return m_capacity; }
    size_t size() const { return m_size; }
    size_t available() const { return m_capacity - m_size; }
    
    bool empty() const { return m_size == 0; }
    bool full() const { return m_size >= m_capacity; }
    
    // Reserve additional capacity
    bool reserve(size_t additional) {
        if (additional == 0) return true;
        
        size_t newSize = m_size + additional;
        if (newSize <= m_capacity) {
            return true;  // Already have enough space
        }
        
        // Need to grow
        return grow(newSize);
    }
    
    // Ensure minimum capacity
    void ensureCapacity(size_t minCapacity) {
        if (minCapacity > m_capacity) {
            grow(minCapacity);
        }
    }
    
    // =========================================================================
    // Data Access
    // =========================================================================
    
    // Raw data pointer (read-only)
    const uint8_t* data() const { return m_data.data(); }
    
    // Raw data pointer (writable)
    uint8_t* mutableData() { return m_data.data(); }
    
    // Access byte at position (read-only)
    uint8_t operator[](size_t index) const {
        if (index >= m_size) {
            throw BufferException("Buffer index out of bounds");
        }
        return m_data[index];
    }
    
    // Access byte at position (writable)
    uint8_t& operator[](size_t index) {
        if (index >= m_size) {
            throw BufferException("Buffer index out of bounds");
        }
        return m_data[index];
    }
    
    // Get view of buffer content
    std::span<const uint8_t> span() const {
        return std::span<const uint8_t>(m_data.data(), m_size);
    }
    
    // Get mutable view of buffer content
    std::span<uint8_t> mutableSpan() {
        return std::span<uint8_t>(m_data.data(), m_size);
    }
    
    // Get substring as string
    std::string_view toStringView() const {
        return std::string_view(reinterpret_cast<const char*>(m_data.data()), m_size);
    }
    
    std::string toString() const {
        return std::string(reinterpret_cast<const char*>(m_data.data()), m_size);
    }
    
    // =========================================================================
    // Writing Data
    // =========================================================================
    
    // Append raw bytes
    void write(const void* src, size_t size) {
        if (!src || size == 0) return;
        
        if (m_size + size > m_capacity) {
            if (!grow(m_size + size)) {
                throw BufferException("Failed to grow buffer - maximum capacity exceeded");
            }
        }
        
        std::memcpy(m_data.data() + m_size, src, size);
        m_size += size;
    }
    
    // Append from span
    void write(std::span<const uint8_t> data) {
        write(data.data(), data.size());
    }
    
    // Append from vector
    void write(const std::vector<uint8_t>& data) {
        if (!data.empty()) {
            write(data.data(), data.size());
        }
    }
    
    // Append from string
    void write(std::string_view str) {
        write(str.data(), str.size());
    }
    
    // Append single byte
    void writeByte(uint8_t byte) {
        if (m_size >= m_capacity) {
            if (!grow(m_size + 1)) {
                throw BufferException("Failed to grow buffer - maximum capacity exceeded");
            }
        }
        m_data[m_size++] = byte;
    }
    
    // =========================================================================
    // Reading Data
    // =========================================================================
    
    // Read and remove from front
    void read(void* dest, size_t size) {
        if (!dest) {
            throw BufferException("Null destination buffer");
        }
        if (size == 0) return;
        
        if (size > m_size) {
            throw BufferException("Read exceeds buffer size");
        }
        
        std::memcpy(dest, m_data.data(), size);
        remove(size);
    }
    
    // Read as span (doesn't remove)
    std::span<const uint8_t> peek(size_t size) const {
        if (size > m_size) {
            size = m_size;
        }
        return std::span<const uint8_t>(m_data.data(), size);
    }
    
    // Peek as string view
    std::string_view peekString(size_t size) const {
        if (size > m_size) {
            size = m_size;
        }
        return std::string_view(reinterpret_cast<const char*>(m_data.data()), size);
    }
    
    // Get first byte without removing
    std::optional<uint8_t> peekByte() const {
        if (m_size == 0) return std::nullopt;
        return m_data[0];
    }
    
    // =========================================================================
    // Removing Data
    // =========================================================================
    
    // Remove n bytes from front (shift data)
    void remove(size_t count) {
        if (count >= m_size) {
            clear();
            return;
        }
        
        // Shift remaining data to front
        std::memmove(m_data.data(), m_data.data() + count, m_size - count);
        m_size -= count;
    }
    
    // Remove all data
    void clear() {
        m_size = 0;
    }
    
    // Reset to zero capacity state
    void reset() {
        m_size = 0;
        m_capacity = MIN_CAPACITY;
        m_data.resize(m_capacity);
    }
    
    // =========================================================================
    // Searching
    // =========================================================================
    
    // Find pattern in buffer
    std::optional<size_t> find(const void* pattern, size_t patternSize, size_t start = 0) const {
        if (!pattern || patternSize == 0 || start >= m_size) {
            return std::nullopt;
        }
        
        const uint8_t* p = static_cast<const uint8_t*>(pattern);
        
        for (size_t i = start; i + patternSize <= m_size; ++i) {
            if (std::memcmp(m_data.data() + i, p, patternSize) == 0) {
                return i;
            }
        }
        
        return std::nullopt;
    }
    
    // Find byte sequence (convenience)
    std::optional<size_t> find(std::string_view pattern, size_t start = 0) const {
        return find(pattern.data(), pattern.size(), start);
    }
    
    // Find CRLF sequence
    std::optional<size_t> findCrlf(size_t start = 0) const {
        for (size_t i = start; i + 1 < m_size; ++i) {
            if (m_data[i] == '\r' && m_data[i + 1] == '\n') {
                return i;
            }
        }
        return std::nullopt;
    }
    
    // Find double CRLF (end of HTTP headers)
    std::optional<size_t> findDoubleCrlf(size_t start = 0) const {
        for (size_t i = start; i + 3 < m_size; ++i) {
            if (m_data[i] == '\r' && m_data[i + 1] == '\n' && 
                m_data[i + 2] == '\r' && m_data[i + 3] == '\n') {
                return i;
            }
        }
        return std::nullopt;
    }
    
    // Check if buffer starts with pattern
    bool startsWith(const void* pattern, size_t size) const {
        if (!pattern || size == 0 || size > m_size) {
            return false;
        }
        return std::memcmp(m_data.data(), pattern, size) == 0;
    }
    
    bool startsWith(std::string_view pattern) const {
        return startsWith(pattern.data(), pattern.size());
    }
    
    // =========================================================================
    // Utility
    // =========================================================================
    
    // Swap contents with another buffer
    void swap(ByteBuffer& other) noexcept {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }
    
    // Create a copy of current contents
    ByteBuffer clone() const {
        ByteBuffer copy(m_capacity);
        copy.m_size = m_size;
        if (m_size > 0) {
            std::memcpy(copy.m_data.data(), m_data.data(), m_size);
        }
        return copy;
    }
    
    // Truncate buffer to specified size
    void truncate(size_t newSize) {
        if (newSize < m_size) {
            m_size = newSize;
        }
    }
    
    // Append another buffer
    void append(const ByteBuffer& other) {
        if (!other.empty()) {
            write(other.data(), other.size());
        }
    }
    
    // Get stats for debugging
    struct Stats {
        size_t size;
        size_t capacity;
        size_t utilizationPercent;
        size_t freeSpace;
    };
    
    Stats stats() const {
        return Stats{
            m_size,
            m_capacity,
            m_capacity > 0 ? (m_size * 100) / m_capacity : 0,
            m_capacity - m_size
        };
    }
    
    // Validate buffer integrity
    bool isValid() const {
        return m_size <= m_capacity && m_data.size() == m_capacity;
    }
    
private:
    std::vector<uint8_t> m_data;
    size_t m_size = 0;
    size_t m_capacity = MIN_CAPACITY;
    
    // Grow buffer to accommodate required size
    bool grow(size_t requiredCapacity) {
        if (requiredCapacity > MAX_CAPACITY) {
            return false;  // Would exceed maximum
        }
        
        // Double growth strategy
        size_t newCapacity = m_capacity;
        while (newCapacity < requiredCapacity) {
            newCapacity *= 2;
        }
        
        // Cap at MAX_CAPACITY
        if (newCapacity > MAX_CAPACITY) {
            newCapacity = MAX_CAPACITY;
            if (requiredCapacity > MAX_CAPACITY) {
                return false;  // Can't accommodate
            }
        }
        
        m_data.resize(newCapacity);
        m_capacity = newCapacity;
        return true;
    }
};

// ============================================================================
// Buffer Utilities
// ============================================================================

namespace BufferUtil {
    
// Convert buffer to hex string
std::string toHex(const ByteBuffer& buffer, size_t maxBytes = 0);

// Create buffer from hex string
ByteBuffer fromHex(const std::string& hex);

// Check if buffer appears to be text (no null bytes in first n bytes)
bool isTextual(const ByteBuffer& buffer, size_t checkBytes = 256);

// Convert text portion to string, replacing invalid chars
std::string toPrintableString(const ByteBuffer& buffer, size_t maxBytes = 0);

// Calculate checksum (simple sum for debugging)
uint32_t checksum(const ByteBuffer& buffer);

} // namespace BufferUtil

} // namespace OmniNet

#endif // OMNINET_BUFFER_H