// OmniNet - Static File Handler
#ifndef OMNINET_STATIC_HANDLER_H
#define OMNINET_STATIC_HANDLER_H

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

namespace OmniNet::HTTP {

// ============================================================================
// Static File Handler
// ============================================================================

class StaticHandler {
public:
    // Construct with root directory
    explicit StaticHandler(std::string rootDir);
    
    // Handle request (returns true if file was served)
    bool handle(const Request& req, Response& resp);
    
    // --- Configuration ---
    
    // Default file when directory is requested
    void setDefaultFile(std::string filename);
    std::string_view defaultFile() const;
    
    // Index files (in order of preference)
    void setIndexFiles(std::vector<std::string> files);
    const std::vector<std::string>& indexFiles() const;
    
    // Allow directory listing
    void allowDirectoryListing(bool allow);
    bool directoryListingAllowed() const;
    
    // Set cache max age
    void setCacheMaxAge(std::chrono::seconds maxAge);
    std::chrono::seconds cacheMaxAge() const;
    
    // --- MIME Types ---
    
    // Get MIME type from file extension
    static std::string mimeType(std::string_view path);
    
    // Register custom MIME type
    static void setMimeType(std::string_view extension, std::string_view mimeType);
    
private:
    // Resolve URL path to filesystem path
    std::string resolvePath(std::string_view urlPath) const;
    
    // Check if path is hidden
    bool isHidden(std::string_view path) const;
    
    // Check for path traversal attacks
    bool isSafePath(std::string_view path) const;
    
    // Serve file
    Response serveFile(const std::string& realPath, const std::string& urlPath);
    
    // Serve directory
    Response serveDirectory(const std::string& dirPath, const std::string& urlPath);
    
    // Serve index file
    Response serveIndex(const std::string& dirPath, const std::string& urlPath);
    
    // Generate directory listing HTML
    std::string generateDirectoryListing(
        const std::string& dirPath,
        const std::string& urlPath,
        const std::vector<std::string>& entries
    );
    
    std::string m_rootDir;
    std::vector<std::string> m_indexFiles = {"index.html", "index.htm"};
    bool m_allowDirectoryListing = false;
    std::chrono::seconds m_cacheMaxAge{3600};  // 1 hour
    
    static std::unordered_map<std::string, std::string> s_mimeTypes;
    static std::once_flag s_mimeTypesInit;
    
    static void initMimeTypes();
};

// ============================================================================
// Static File Server (convenience class)
// ============================================================================

class StaticServer : public StaticHandler {
public:
    StaticServer(std::string urlPrefix, std::string rootDir);
    
    void handleRequest(const Request& req, Response& resp);
    
private:
    std::string m_urlPrefix;
};

} // namespace OmniNet::HTTP

#endif // OMNINET_STATIC_HANDLER_H
