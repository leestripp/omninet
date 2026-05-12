// OmniNet - Static File Handler Implementation
#include "StaticHandler.h"
#include "utils/Utils.h"
#include "utils/Logger.h"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace OmniNet::HTTP {
using OmniNet::Utils::urlDecode;
using OmniNet::Utils::normalizePath;
using OmniNet::Utils::filenameOf;
using OmniNet::Utils::extensionOf;
using OmniNet::Utils::readFile;
using OmniNet::Utils::escapeHtml;
using OmniNet::Utils::directoryOf;
using OmniNet::Logger;

// ============================================================================
// Static Handler
// ============================================================================

std::unordered_map<std::string, std::string> StaticHandler::s_mimeTypes;
std::once_flag StaticHandler::s_mimeTypesInit;

StaticHandler::StaticHandler(std::string rootDir)
    : m_rootDir(std::move(rootDir)) {
    initMimeTypes();
}

void StaticHandler::initMimeTypes() {
    std::call_once(s_mimeTypesInit, []() {
        s_mimeTypes = {
            {".html", "text/html"},
            {".htm", "text/html"},
            {".css", "text/css"},
            {".js", "application/javascript"},
            {".mjs", "application/javascript"},
            {".json", "application/json"},
            {".xml", "application/xml"},
            {".txt", "text/plain"},
            {".csv", "text/csv"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".bmp", "image/bmp"},
            {".webp", "image/webp"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            {".pdf", "application/pdf"},
            {".zip", "application/zip"},
            {".gz", "application/gzip"},
            {".tar", "application/x-tar"},
            {".mp3", "audio/mpeg"},
            {".wav", "audio/wav"},
            {".mp4", "video/mp4"},
            {".webm", "video/webm"},
            {".woff", "font/woff"},
            {".woff2", "font/woff2"},
            {".ttf", "font/ttf"},
            {".otf", "font/otf"},
            {".eot", "application/vnd.ms-fontobject"},
        };
    });
}

bool StaticHandler::handle(const Request& req, Response& resp) {
    // Only handle GET/HEAD
    if (req.method() != Method::GET && req.method() != Method::HEAD) {
        return false;
    }
    
    std::string path;
    try {
        path = resolvePath(req.path());
        Logger::instance().debugF("[StaticHandler] Resolved path: '%s' -> '%s'", std::string(req.path()).c_str(), path.c_str());
    } catch (const std::exception& e) {
        Logger::instance().errorF("[StaticHandler] Failed to resolve path '%s': %s", std::string(req.path()).c_str(), e.what());
        resp = Response::internalError("Failed to resolve path");
        return true;  // Return true to indicate we handled the request (with error)
    }
    
    if (isHidden(path)) {
        Logger::instance().debugF("[StaticHandler] Path is hidden: %s", path.c_str());
        resp = Response::notFound();
        return false;
    }
    
    std::filesystem::path fsPath(path);
    
    try {
        if (std::filesystem::is_directory(fsPath)) {
            Logger::instance().debugF("[StaticHandler] Path is directory: %s", path.c_str());
            // Try index files
            for (const auto& index : m_indexFiles) {
                auto indexPath = fsPath / index;
                if (std::filesystem::exists(indexPath)) {
                    Logger::instance().debugF("[StaticHandler] Found index file: %s", indexPath.string().c_str());
                    resp = serveFile(indexPath.string(), std::string(req.path()));
                    return true;
                }
            }
            
            // Directory listing?
            if (m_allowDirectoryListing) {
                resp = serveDirectory(path, std::string(req.path()));
                return true;
            }
            
            Logger::instance().debug(std::string_view("[StaticHandler] Directory has no index file and listing disabled"));
            resp = Response::notFound();
            return false;
        }
        
        if (std::filesystem::is_regular_file(fsPath)) {
            Logger::instance().debugF("[StaticHandler] Path is regular file: %s", path.c_str());
            resp = serveFile(path, std::string(req.path()));
            return true;
        }
        
        Logger::instance().debugF("[StaticHandler] Path does not exist or is not accessible: %s", path.c_str());
        resp = Response::notFound();
    } catch (const std::exception& e) {
        Logger::instance().errorF("[StaticHandler] Filesystem error for path '%s': %s", path.c_str(), e.what());
        resp = Response::internalError("Filesystem error: " + std::string(e.what()));
        return true;  // Return true to indicate we handled the request (with error)
    }
    
    return false;
}

std::string StaticHandler::resolvePath(std::string_view urlPath) const {
    std::string path;
    
    // Decode URL path
    auto decoded = Utils::urlDecode(urlPath);
    
    // Normalize and make absolute
    if (decoded.empty() || decoded == "/") {
        path = m_rootDir;
    } else {
        // Remove leading slash
        if (decoded[0] == '/') {
            decoded = decoded.substr(1);
        }
        
        path = m_rootDir + "/" + decoded;
    }
    
    // Normalize path (resolve .. and .)
    return Utils::normalizePath(path);
}

bool StaticHandler::isHidden(std::string_view path) const {
    auto filename = Utils::filenameOf(path);
    return filename.starts_with(".");
}

std::string StaticHandler::mimeType(std::string_view path) {
    initMimeTypes();
    auto ext = Utils::extensionOf(path);
    auto it = s_mimeTypes.find(std::string(ext));
    if (it != s_mimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

void StaticHandler::setMimeType(std::string_view extension, std::string_view mimeType) {
    s_mimeTypes[std::string(extension)] = std::string(mimeType);
}

void StaticHandler::setDefaultFile(std::string filename) {
    m_indexFiles.insert(m_indexFiles.begin(), std::move(filename));
}

std::string_view StaticHandler::defaultFile() const {
    return m_indexFiles.empty() ? "" : m_indexFiles[0];
}

const std::vector<std::string>& StaticHandler::indexFiles() const {
    return m_indexFiles;
}

void StaticHandler::setIndexFiles(std::vector<std::string> files) {
    m_indexFiles = std::move(files);
}

void StaticHandler::allowDirectoryListing(bool allow) {
    m_allowDirectoryListing = allow;
}

bool StaticHandler::directoryListingAllowed() const {
    return m_allowDirectoryListing;
}

void StaticHandler::setCacheMaxAge(std::chrono::seconds maxAge) {
    m_cacheMaxAge = maxAge;
}

std::chrono::seconds StaticHandler::cacheMaxAge() const {
    return m_cacheMaxAge;
}

Response StaticHandler::serveFile(const std::string& realPath, const std::string& urlPath) {
    Response resp;
    
    Logger::instance().debugF("[StaticHandler] serveFile: realPath='%s', urlPath='%s'", realPath.c_str(), urlPath.c_str());
    
    // Check if file exists first
    if (!std::filesystem::exists(realPath)) {
        Logger::instance().warnF("[StaticHandler] File does not exist: %s", realPath.c_str());
        return Response::notFound();
    }
    
    // Read the file
    try {
        auto content = Utils::readFile(realPath);
        if (!content) {
            Logger::instance().warnF("[StaticHandler] Failed to read file: %s", realPath.c_str());
            return Response::notFound();
        }
        
        Logger::instance().debugF("[StaticHandler] Successfully read %zu bytes from %s", content->size(), realPath.c_str());
        
        auto etag = ResponseUtil::computeETag(*content);
        resp.setETag(etag);
        resp.setContentType(mimeType(realPath));
        resp.setBody(std::move(*content));
        resp.setCacheControl("max-age=" + std::to_string(m_cacheMaxAge.count()));
        resp.setStatus(StatusCode::OK);
        
        return resp;
    } catch (const std::exception& e) {
        Logger::instance().errorF("[StaticHandler] Error serving file '%s': %s", realPath.c_str(), e.what());
        return Response::internalError("Error reading file: " + std::string(e.what()));
    }
}

Response StaticHandler::serveDirectory(const std::string& dirPath, const std::string& urlPath) {
    Response resp;
    resp.setStatus(StatusCode::OK);
    resp.setContentType("text/html; charset=utf-8");
    resp.setBody(generateDirectoryListing(dirPath, urlPath, {}));
    return resp;
}

Response StaticHandler::serveIndex(const std::string& dirPath, const std::string& urlPath) {
    return serveDirectory(dirPath, urlPath);
}

std::string StaticHandler::generateDirectoryListing(
    const std::string& dirPath,
    const std::string& urlPath,
    const std::vector<std::string>& entries
) {
    std::ostringstream html;
    html << "<!DOCTYPE html>\n";
    html << "<html><head><title>Index of " << Utils::escapeHtml(urlPath) << "</title></head>\n";
    html << "<body><h1>Index of " << Utils::escapeHtml(urlPath) << "</h1>\n";
    html << "<ul>\n";
    
    // Parent directory link
    if (urlPath != "/") {
        auto parent = Utils::directoryOf(urlPath);
        if (parent.empty()) parent = "/";
        html << "<li><a href=\"" << parent << "\">..</a></li>\n";
    }
    
    html << "</ul>\n";
    html << "<hr><pre>OmniNet Static Server</pre>\n";
    html << "</body></html>\n";
    
    return html.str();
}

// ============================================================================
// Static Server
// ============================================================================

StaticServer::StaticServer(std::string urlPrefix, std::string rootDir)
    : StaticHandler(std::move(rootDir)), m_urlPrefix(std::move(urlPrefix)) {}

void StaticServer::handleRequest(const Request& req, Response& resp) {
    auto path = req.path();
    
    // Check prefix
    if (!m_urlPrefix.empty() && !path.starts_with(m_urlPrefix)) {
        return;
    }
    
    // Strip prefix
    if (!m_urlPrefix.empty()) {
        path = path.substr(m_urlPrefix.size());
    }
    
    handle(req, resp);
}

} // namespace OmniNet::HTTP