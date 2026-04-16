// OmniNet - Rate Limiter Implementation
#include "RateLimiter.h"

namespace OmniNet {

// ============================================================================
// Rate Limiter
// ============================================================================

RateLimiter::RateLimiter(size_t maxAttempts, std::chrono::seconds windowSeconds)
    : m_maxAttempts(maxAttempts)
    , m_windowSeconds(windowSeconds)
    , m_blockDuration(windowSeconds * 2) {}

bool RateLimiter::allow(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto it = m_attempts.find(key);
    
    if (it == m_attempts.end()) {
        m_attempts[key] = {1, now, {}};
        return true;
    }
    
    // Check if blocked
    if (it->second.blockedUntil > now) {
        return false;
    }
    
    // Check window
    auto windowStart = it->second.windowStart;
    if (now - windowStart >= m_windowSeconds) {
        // Window expired, reset
        it->second.count = 1;
        it->second.windowStart = now;
        return true;
    }
    
    // Within window
    if (it->second.count >= m_maxAttempts) {
        // Block
        it->second.blockedUntil = now + m_blockDuration;
        return false;
    }
    
    ++it->second.count;
    return true;
}

void RateLimiter::recordFailure(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto it = m_attempts.find(key);
    
    if (it == m_attempts.end()) {
        m_attempts[key] = {1, now, {}};
    } else {
        ++it->second.count;
    }
}

void RateLimiter::recordSuccess(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_attempts.erase(key);
}

bool RateLimiter::isBlocked(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_attempts.find(key);
    if (it == m_attempts.end()) return false;
    
    return it->second.blockedUntil > std::chrono::steady_clock::now();
}

size_t RateLimiter::remainingAttempts(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_attempts.find(key);
    if (it == m_attempts.end()) return m_maxAttempts;
    
    return m_maxAttempts - it->second.count;
}

std::chrono::seconds RateLimiter::blockTimeRemaining(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_attempts.find(key);
    if (it == m_attempts.end()) return std::chrono::seconds::zero();
    
    auto now = std::chrono::steady_clock::now();
    if (it->second.blockedUntil <= now) return std::chrono::seconds::zero();
    
    return std::chrono::duration_cast<std::chrono::seconds>(
        it->second.blockedUntil - now
    );
}

void RateLimiter::clear(const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_attempts.erase(key);
}

void RateLimiter::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_attempts.clear();
}

size_t RateLimiter::maxAttempts() const {
    return m_maxAttempts;
}

void RateLimiter::setMaxAttempts(size_t max) {
    m_maxAttempts = max;
}

std::chrono::seconds RateLimiter::windowSeconds() const {
    return m_windowSeconds;
}

void RateLimiter::setWindowSeconds(std::chrono::seconds seconds) {
    m_windowSeconds = seconds;
}

std::chrono::seconds RateLimiter::blockDuration() const {
    return m_blockDuration;
}

void RateLimiter::setBlockDuration(std::chrono::seconds duration) {
    m_blockDuration = duration;
}

// ============================================================================
// Token Bucket
// ============================================================================

TokenBucket::TokenBucket(double rate, size_t capacity)
    : m_rate(rate)
    , m_capacity(capacity)
    , m_tokens(capacity)
    , m_lastRefill(std::chrono::steady_clock::now()) {}

bool TokenBucket::tryConsume(size_t tokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    refill();
    
    if (m_tokens >= tokens) {
        m_tokens -= tokens;
        return true;
    }
    
    return false;
}

size_t TokenBucket::available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tokens;
}

void TokenBucket::refill(size_t tokens) {
    m_tokens = std::min(m_tokens + tokens, m_capacity);
}

void TokenBucket::refill() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_lastRefill).count();
    
    double newTokens = elapsed * m_rate;
    m_tokens = std::min(m_tokens + static_cast<size_t>(newTokens), m_capacity);
    m_lastRefill = now;
}

// ============================================================================
// Sliding Window
// ============================================================================

SlidingWindow::SlidingWindow(size_t maxHits, std::chrono::seconds windowSeconds)
    : m_maxHits(maxHits), m_windowSeconds(windowSeconds) {}

void SlidingWindow::record() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    m_hits.push_back({now});
}

size_t SlidingWindow::hits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - m_windowSeconds;
    
    // Remove old hits
    size_t count = 0;
    for (const auto& hit : m_hits) {
        if (hit.time > cutoff) {
            ++count;
        }
    }
    
    return count;
}

bool SlidingWindow::isAtLimit() const {
    return hits() >= m_maxHits;
}

void SlidingWindow::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hits.clear();
}

// ============================================================================
// IP Rate Limiter
// ============================================================================

IPRateLimiter::IPRateLimiter(size_t maxRequests, std::chrono::seconds window)
    : m_window(maxRequests, window) {}

bool IPRateLimiter::isLimited(const std::string& ip) {
    return m_window.isAtLimit();
}

void IPRateLimiter::record(const std::string& ip) {
    (void)ip;
    m_window.record();
}

size_t IPRateLimiter::remaining(const std::string& ip) const {
    (void)ip;
    return m_maxHits - m_window.hits();
}

bool IPRateLimiter::operator()(const HTTP::Request& req, HTTP::Response& resp) {
    auto ip = req.remoteAddress();
    
    if (isLimited(ip)) {
        resp = HTTP::Response::tooManyRequests();
        return false;
    }
    
    record(ip);
    return true;
}

} // namespace OmniNet