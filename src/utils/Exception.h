// OmniNet - Exception hierarchy
#ifndef OMNINET_EXCEPTION_H
#define OMNINET_EXCEPTION_H

#include <exception>
#include <string>
#include <string_view>

namespace OmniNet {

// ============================================================================
// Base Exception
// ============================================================================

class Exception : public std::exception {
public:
    explicit Exception(const std::string& message, int errorCode = 0);
    explicit Exception(std::string_view message, int errorCode = 0);
    
    const char* what() const noexcept override;
    int code() const noexcept;
    
protected:
    std::string m_message;
    int m_code = 0;
};

// ============================================================================
// I/O Related Exceptions
// ============================================================================

class IOException : public Exception {
public:
    explicit IOException(const std::string& message, int errorCode = 0);
    explicit IOException(std::string_view message, int errorCode = 0);
};

class SocketException : public IOException {
public:
    explicit SocketException(const std::string& message, int errorCode = 0);
    explicit SocketException(std::string_view message, int errorCode = 0);
};

class TLSException : public IOException {
public:
    explicit TLSException(const std::string& message, int errorCode = 0);
    explicit TLSException(std::string_view message, int errorCode = 0);
};

class FileException : public IOException {
public:
    explicit FileException(const std::string& message, int errorCode = 0);
    explicit FileException(std::string_view message, int errorCode = 0);
};

// ============================================================================
// Parsing Exceptions
// ============================================================================

class ParseException : public Exception {
public:
    explicit ParseException(const std::string& message, int errorCode = 0);
    explicit ParseException(std::string_view message, int errorCode = 0);
    
    // Optional: position where parse error occurred
    size_t position() const;
    void setPosition(size_t pos);
    
private:
    size_t m_position = 0;
};

// ============================================================================
// Database Exceptions
// ============================================================================

class DatabaseException : public Exception {
public:
    explicit DatabaseException(const std::string& message, int errorCode = 0);
    explicit DatabaseException(std::string_view message, int errorCode = 0);
};

// ============================================================================
// HTTP Related Exceptions
// ============================================================================

class HttpException : public Exception {
public:
    explicit HttpException(const std::string& message, int statusCode = 500);
    explicit HttpException(std::string_view message, int statusCode = 500);
    
    int statusCode() const noexcept;
    
private:
    int m_statusCode = 500;
};

class RouterException : public Exception {
public:
    explicit RouterException(const std::string& message, int errorCode = 0);
    explicit RouterException(std::string_view message, int errorCode = 0);
};

// ============================================================================
// Authentication/Authorization Exceptions
// ============================================================================

class AuthException : public Exception {
public:
    explicit AuthException(const std::string& message, int errorCode = 401);
    explicit AuthException(std::string_view message, int errorCode = 401);
};

class PermissionException : public Exception {
public:
    explicit PermissionException(const std::string& message, int errorCode = 403);
    explicit PermissionException(std::string_view message, int errorCode = 403);
};

// ============================================================================
// Validation Exceptions
// ============================================================================

class ValidationException : public Exception {
public:
    explicit ValidationException(const std::string& message, int errorCode = 400);
    explicit ValidationException(std::string_view message, int errorCode = 400);
};

// ============================================================================
// Implementation
// ============================================================================

// Base Exception implementation
inline Exception::Exception(const std::string& message, int errorCode)
    : m_message(message), m_code(errorCode) {}

inline Exception::Exception(std::string_view message, int errorCode)
    : m_message(std::string(message)), m_code(errorCode) {}

inline const char* Exception::what() const noexcept {
    return m_message.c_str();
}

inline int Exception::code() const noexcept {
    return m_code;
}

// IOException implementation
inline IOException::IOException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline IOException::IOException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

// SocketException implementation
inline SocketException::SocketException(const std::string& message, int errorCode)
    : IOException(message, errorCode) {}

inline SocketException::SocketException(std::string_view message, int errorCode)
    : IOException(message, errorCode) {}

// TLSException implementation
inline TLSException::TLSException(const std::string& message, int errorCode)
    : IOException(message, errorCode) {}

inline TLSException::TLSException(std::string_view message, int errorCode)
    : IOException(message, errorCode) {}

// FileException implementation
inline FileException::FileException(const std::string& message, int errorCode)
    : IOException(message, errorCode) {}

inline FileException::FileException(std::string_view message, int errorCode)
    : IOException(message, errorCode) {}

// ParseException implementation
inline ParseException::ParseException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline ParseException::ParseException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

inline size_t ParseException::position() const {
    return m_position;
}

inline void ParseException::setPosition(size_t pos) {
    m_position = pos;
}

// DatabaseException implementation
inline DatabaseException::DatabaseException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline DatabaseException::DatabaseException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

// HttpException implementation
inline HttpException::HttpException(const std::string& message, int statusCode)
    : Exception(message, statusCode), m_statusCode(statusCode) {}

inline HttpException::HttpException(std::string_view message, int statusCode)
    : Exception(message, statusCode), m_statusCode(statusCode) {}

inline int HttpException::statusCode() const noexcept {
    return m_statusCode;
}

// RouterException implementation
inline RouterException::RouterException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline RouterException::RouterException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

// AuthException implementation
inline AuthException::AuthException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline AuthException::AuthException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

// PermissionException implementation
inline PermissionException::PermissionException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline PermissionException::PermissionException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

// ValidationException implementation
inline ValidationException::ValidationException(const std::string& message, int errorCode)
    : Exception(message, errorCode) {}

inline ValidationException::ValidationException(std::string_view message, int errorCode)
    : Exception(message, errorCode) {}

} // namespace OmniNet

#endif // OMNINET_EXCEPTION_H
