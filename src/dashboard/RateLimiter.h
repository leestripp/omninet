// OmniNet - Rate Limiter
#ifndef OMNINET_RATE_LIMITER_H
#define OMNINET_RATE_LIMITER_H

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace OmniNet {

// ============================================================================
// Rate Limiter
// ============================================================================

class RateLimiter {
public:
    struct Attempt {
        size_t count = 0;
        std::chrono::steady_clock::time_point windowStart;
        std::chrono::steady_clock::time_point blockedUntil;
    };
    
    // Construct with max attempts and time window
    RateLimiter(size_t maxAttempts, std::chrono::seconds windowSeconds);
    
    // Check if action is allowed for key
    bool allow(const std::string& key);
    
    // Record failed attempt (increments counter)
    void recordFailure(const std::string& key);
    
    // Record successful attempt (resets counter)
    void recordSuccess(const std::string& key);
    
    // Check if key is currently blocked
    bool isBlocked(const std::string& key) const;
    
    // Get remaining attempts before block
    size_t remainingAttempts(const std::string& key) const;
    
    // Get time until block expires
    std::chrono::seconds blockTimeRemaining(const std::string& key) const;
    
    // Clear all records for key
    void clear(const std::string& key);
    
    // Clear all records
    void clearAll();
    
    // --- Configuration ---
    
    size_t maxAttempts() const;
    void setMaxAttempts(size_t max);
    
    std::chrono::seconds windowSeconds() const;
    void setWindowSeconds(std::chrono::seconds seconds);
    
    std::chrono::seconds blockDuration() const;
    void setBlockDuration(std::chrono::seconds duration);

private:
    size_t m_maxAttempts;
    std::chrono::seconds m_windowSeconds;
    std::chrono::seconds m_blockDuration;
    
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Attempt> m_attempts;
};

// ============================================================================
// Token Bucket Rate Limiter
// ============================================================================

class TokenBucket {
public:
    // rate = tokens per second, capacity = max tokens
    TokenBucket(double rate, size_t capacity);
    
    // Try to consume tokens
    bool tryConsume(size_t tokens = 1);
    
    // Get current token count
    size_t available() const;
    
    // Refill tokens
    void refill(size_t tokens);
    
    // Refill based on elapsed time
    void refill();
    
private:
    double m_rate;
    size_t m_capacity;
    size_t m_tokens;
    std::chrono::steady_clock::time_point m_lastRefill;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Sliding Window Rate Limiter
// ============================================================================

class SlidingWindow {
public:
    SlidingWindow(size_t maxHits, std::chrono::seconds windowSeconds);
    
    // Record a hit
    void record();
    
    // Get hits in window
    size_t hits() const;
    
    // Check if at limit
    bool isAtLimit() const;
    
    // Reset
    void reset();

private:
    struct Hit {
        std::chrono::steady_clock::time_point time;
    };
    
    size_t m_maxHits;
    std::chrono::seconds m_windowSeconds;
    std::vector<Hit> m_hits;
    mutable std::mutex m_mutex;
};

// ============================================================================
// IP-Based Rate Limiter Middleware
// ============================================================================

class IPRateLimiter {
public:
    IPRateLimiter(size_t maxRequests, std::chrono::seconds window);
    
    // Check if IP is rate limited
    bool isLimited(const std::string& ip);
    
    // Record request from IP
    void record(const std::string& ip);
    
    // Get remaining requests for IP
    size_t remaining(const std::string& ip) const;
    
    // Middleware function
    bool operator()(const HTTP::Request& req, HTTP::Response& resp);

private:
    SlidingWindow m_window;
};

} // namespace OmniNet

#endif // OMNINET_RATE_LIMITER_H
