// OmniNet - std::expected Utilities
// Common error types and helpers for type-safe error handling
#ifndef OMNINET_UTILS_EXPECTED_H
#define OMNINET_UTILS_EXPECTED_H

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>

namespace OmniNet {

// ============================================================================
// Error Codes (Common)
// ============================================================================

enum class ErrorCode : int32_t {
    Success = 0,
    Unknown = -1,
    InvalidArgument = -2,
    NotFound = -3,
    PermissionDenied = -4,
    Timeout = -5,
    AlreadyExists = -6,
    OutOfMemory = -7,
    NotImplemented = -8,
    Cancelled = -9,
};

// ============================================================================
// SocketError
// ============================================================================

enum class SocketError {
    Success = 0,
    Unknown = 1,
    InvalidAddress,
    ConnectionRefused,
    ConnectionReset,
    Timeout,
    NetworkUnreachable,
    HostUnreachable,
    PermissionDenied,
    AddressInUse,
    OutOfMemory,
    NotConnected,
    AlreadyConnected,
    InProgress,
    WouldBlock,
    MessageTooLong,
    PipeBroken,
    NotASocket,
    InvalidSocket,
    SystemError,  // Use errno for details
};

inline std::string_view toString(SocketError err) {
    switch (err) {
        case SocketError::Success:           return "Success";
        case SocketError::Unknown:           return "Unknown error";
        case SocketError::InvalidAddress:    return "Invalid address";
        case SocketError::ConnectionRefused: return "Connection refused";
        case SocketError::ConnectionReset:   return "Connection reset";
        case SocketError::Timeout:            return "Operation timed out";
        case SocketError::NetworkUnreachable: return "Network unreachable";
        case SocketError::HostUnreachable:    return "Host unreachable";
        case SocketError::PermissionDenied:   return "Permission denied";
        case SocketError::AddressInUse:       return "Address already in use";
        case SocketError::OutOfMemory:        return "Out of memory";
        case SocketError::NotConnected:       return "Not connected";
        case SocketError::AlreadyConnected:   return "Already connected";
        case SocketError::InProgress:         return "Operation in progress";
        case SocketError::WouldBlock:         return "Operation would block";
        case SocketError::MessageTooLong:     return "Message too long";
        case SocketError::PipeBroken:         return "Broken pipe";
        case SocketError::NotASocket:         return "Not a socket";
        case SocketError::InvalidSocket:      return "Invalid socket";
        case SocketError::SystemError:        return "System error";
    }
    return "Unknown";
}

// ============================================================================
// DbError
// ============================================================================

enum class DbError {
    Success = 0,
    Unknown = 1,
    NotOpen,
    AlreadyOpen,
    InvalidPath,
    InvalidSql,
    StatementFailed,
    StepFailed,
    BindFailed,
    TypeMismatch,
    OutOfMemory,
    Busy,
    Locked,
    ReadOnly,
    ConstraintViolation,
    SyntaxError,
    NoSuchTable,
    NoSuchColumn,
    CorruptDatabase,
    SystemError,
};

inline std::string_view toString(DbError err) {
    switch (err) {
        case DbError::Success:               return "Success";
        case DbError::Unknown:               return "Unknown error";
        case DbError::NotOpen:               return "Database not open";
        case DbError::AlreadyOpen:           return "Database already open";
        case DbError::InvalidPath:           return "Invalid database path";
        case DbError::InvalidSql:            return "Invalid SQL";
        case DbError::StatementFailed:       return "Statement preparation failed";
        case DbError::StepFailed:            return "Statement execution failed";
        case DbError::BindFailed:            return "Statement binding failed";
        case DbError::TypeMismatch:          return "Type mismatch";
        case DbError::OutOfMemory:           return "Out of memory";
        case DbError::Busy:                  return "Database busy";
        case DbError::Locked:                return "Database locked";
        case DbError::ReadOnly:              return "Database is read-only";
        case DbError::ConstraintViolation:   return "Constraint violation";
        case DbError::SyntaxError:           return "SQL syntax error";
        case DbError::NoSuchTable:           return "No such table";
        case DbError::NoSuchColumn:          return "No such column";
        case DbError::CorruptDatabase:       return "Corrupt database";
        case DbError::SystemError:           return "System error";
    }
    return "Unknown";
}

// ============================================================================
// Result<T, E> - Re-export of std::expected with common utilities
// ============================================================================

// C++23 std::expected is the primary type. We re-export it for convenience
// and provide helper functions for common patterns.
template<typename T, typename E>
using Result = std::expected<T, E>;

// Specialization for void success
template<typename E>
using VoidResult = std::expected<void, E>;

// Helper to create a success result
// Usage: Ok<value_type, error_type>(value)
// Note: std::expected already supports the natural syntax, so this is just
// a thin alias for clarity. Use std::expected<T, E>{value} directly.
template<typename T, typename E, typename V>
constexpr Result<T, E> Ok(V&& value) {
    return Result<T, E>{std::in_place, std::forward<V>(value)};
}

// ============================================================================
// Macros for [[deprecated]] migration
// ============================================================================

// Use OMNINET_DEPRECATED to mark old APIs in favor of new std::expected ones
#ifdef __GNUC__
#define OMNINET_DEPRECATED(msg) [[deprecated(msg)]]
#else
#define OMNINET_DEPRECATED(msg) [[deprecated]]
#endif

} // namespace OmniNet

#endif // OMNINET_UTILS_EXPECTED_H
