// omninet-certs - TLS Certificate Manager for OmniNet
// List, inspect, generate, and manage TLS certificates

#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <regex>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"

#ifdef __linux__
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/opensslv.h>
#endif

namespace fs = std::filesystem;
using namespace OmniNet::Tools;

constexpr const char* TOOL_NAME = "omninet-certs";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string action;            // list, info, check, generate, install
    std::string target;            // certificate name or path
    std::string file;
    int days = 365;
    std::string commonName;
    bool json = false;
    bool verbose = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Certificate Info
// ============================================================================

struct CertInfo {
    std::string filename;
    std::string subject;
    std::string issuer;
    std::string serial;
    std::chrono::system_clock::time_point notBefore;
    std::chrono::system_clock::time_point notAfter;
    std::string commonName;
    bool isValid = false;
    bool isExpired = false;
    int daysUntilExpiry = 0;
};

bool readCertificate(const fs::path& path, CertInfo& info);
std::string formatExpiry(const CertInfo& info);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    auto certsDir = Paths::certsDir();
    
    // Ensure certs directory exists
    if (!fs::exists(certsDir)) {
        fs::create_directories(certsDir);
    }
    
    // Execute action
    if (opts.action == "list") {
        out.header("TLS Certificates");
        
        if (!fs::exists(certsDir)) {
            out.warning("No certificates directory found");
            return 0;
        }
        
        std::vector<CertInfo> certs;
        std::vector<std::string> keys;
        
        // Scan directory
        for (const auto& entry : fs::directory_iterator(certsDir)) {
            auto ext = entry.path().extension().string();
            auto name = entry.path().filename().string();
            
            if (ext == ".crt" || ext == ".pem" || ext == ".cer") {
                CertInfo info;
                info.filename = name;
                if (readCertificate(entry.path(), info)) {
                    certs.push_back(info);
                } else {
                    CertInfo bad;
                    bad.filename = name;
                    bad.commonName = "(invalid)";
                    bad.isValid = false;
                    certs.push_back(bad);
                }
            } else if (ext == ".key") {
                keys.push_back(name);
            }
        }
        
        if (certs.empty()) {
            out.info("No certificates found");
            out.info("Use 'omninet-certs generate' to create one");
            return 0;
        }
        
        Table table;
        table.addColumn("Certificate", 30);
        table.addColumn("Common Name", 20);
        table.addColumn("Expires", 15);
        table.addColumn("Status", 10);
        
        for (const auto& cert : certs) {
            std::string status;
            if (cert.isExpired) {
                status = "EXPIRED";
            } else if (!cert.isValid) {
                status = "INVALID";
            } else if (cert.daysUntilExpiry <= 30) {
                status = "EXPIRING";
            } else {
                status = "VALID";
            }
            
            table.addRow({
                cert.filename,
                cert.commonName,
                formatExpiry(cert),
                status
            });
        }
        
        table.render(std::cout);
        
        std::cout << '\n';
        out.kv("Certificates", certs.size());
        out.kv("Keys", keys.size());
        
    } else if (opts.action == "info") {
        if (opts.target.empty()) {
            std::cerr << "Error: Certificate name or path required\n";
            return 1;
        }
        
        fs::path certPath;
        if (fs::exists(opts.target)) {
            certPath = opts.target;
        } else {
            certPath = certsDir / opts.target;
            // Try different extensions
            if (!fs::exists(certPath)) {
                certPath = certsDir / (opts.target + ".crt");
            }
            if (!fs::exists(certPath)) {
                certPath = certsDir / (opts.target + ".pem");
            }
        }
        
        if (!fs::exists(certPath)) {
            std::cerr << "Error: Certificate not found: " << opts.target << '\n';
            return 1;
        }
        
        CertInfo info;
        if (!readCertificate(certPath, info)) {
            std::cerr << "Error: Failed to read certificate\n";
            return 1;
        }
        
        out.header("Certificate: " + info.filename);
        out.kv("Path", certPath.string());
        out.kv("Common Name", info.commonName);
        out.kv("Subject", info.subject);
        out.kv("Issuer", info.issuer);
        out.kv("Serial", info.serial);
        
        std::cout << '\n';
        
        auto notBefore = std::chrono::system_clock::to_time_t(info.notBefore);
        auto notAfter = std::chrono::system_clock::to_time_t(info.notAfter);
        
        out.kv("Valid From", formatTimestamp(notBefore));
        out.kv("Valid Until", formatTimestamp(notAfter));
        out.kv("Days Until Expiry", info.daysUntilExpiry);
        
        std::cout << '\n';
        
        if (info.isExpired) {
            out.error("Certificate is EXPIRED");
        } else if (info.daysUntilExpiry <= 30) {
            out.warning("Certificate expires soon (" + std::to_string(info.daysUntilExpiry) + " days)");
        } else {
            out.success("Certificate is valid");
        }
        
    } else if (opts.action == "check") {
        out.header("Certificate Expiry Check");
        
        if (!fs::exists(certsDir)) {
            out.warning("No certificates directory");
            return 0;
        }
        
        bool allGood = true;
        
        for (const auto& entry : fs::directory_iterator(certsDir)) {
            auto ext = entry.path().extension().string();
            if (ext == ".crt" || ext == ".pem" || ext == ".cer") {
                CertInfo info;
                if (readCertificate(entry.path(), info)) {
                    if (info.isExpired) {
                        out.error(info.filename + " is EXPIRED");
                        allGood = false;
                    } else if (info.daysUntilExpiry <= 30) {
                        out.warning(info.filename + " expires in " + 
                                  std::to_string(info.daysUntilExpiry) + " days");
                        allGood = false;
                    } else {
                        out.success(info.filename + " is valid (" + 
                                  std::to_string(info.daysUntilExpiry) + " days)");
                    }
                }
            }
        }
        
        if (allGood) {
            std::cout << '\n';
            out.success("All certificates are valid and not expiring soon");
        }
        
    } else if (opts.action == "generate") {
        if (opts.target.empty()) {
            opts.target = "omninet";
        }
        if (opts.commonName.empty()) {
            opts.commonName = "localhost";
        }
        
        out.info("Generating self-signed certificate...");
        out.info("Note: Full certificate generation requires OpenSSL integration");
        out.info("Use scripts/generate-certs.sh for full generation");
        
        // TODO: Implement actual certificate generation with OpenSSL
        
    } else if (opts.action == "install") {
        out.info("Certificate installation requires file path");
        out.info("Use 'omninet-certs info <file>' to verify a certificate first");
        
    } else {
        // Default: show help
        std::cout << "Usage: omninet-certs <command> [options]\n";
        std::cout << '\n';
        std::cout << "TLS certificate management for OmniNet.\n";
        std::cout << '\n';
        std::cout << "Commands:\n";
        std::cout << "  list                  List all certificates\n";
        std::cout << "  info <cert>          Show certificate details\n";
        std::cout << "  check                Check expiry dates\n";
        std::cout << "  generate <name>     Generate self-signed cert\n";
        std::cout << "  install <file>      Install certificate\n";
        std::cout << '\n';
        std::cout << "Options:\n";
        std::cout << "  -h, --help          Show this help\n";
        std::cout << "  -d, --days          Validity period (default: 365)\n";
        std::cout << "  -n, --name          Common name (default: localhost)\n";
        std::cout << "  -j, --json          Output as JSON\n";
        std::cout << "  -v, --verbose       Verbose output\n";
    }
    
    return 0;
}

// ============================================================================
// Argument Parsing
// ============================================================================

Options parseArgs(int argc, char* argv[]) {
    Options opts;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            std::cout << TOOL_NAME << " - OmniNet Certificate Manager\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " <command> [options]\n";
            std::cout << '\n';
            std::cout << "TLS certificate management for OmniNet.\n";
            std::cout << '\n';
            std::cout << "Commands:\n";
            std::cout << "  list                   List all certificates\n";
            std::cout << "  info <cert>           Show certificate details\n";
            std::cout << "  check                 Check expiry dates\n";
            std::cout << "  generate <name>      Generate self-signed cert\n";
            std::cout << "  install <file>       Install certificate\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help           Show this help\n";
            std::cout << "  -V, --version        Show version information\n";
            std::cout << "  -d, --days           Validity period (default: 365)\n";
            std::cout << "  -n, --name           Common name (default: localhost)\n";
            std::cout << "  -j, --json           Output as JSON\n";
            std::cout << "  -v, --verbose        Verbose output\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "list" || arg == "info" || arg == "check" ||
                 arg == "generate" || arg == "install") {
            opts.action = arg;
        }
        else if (arg == "-d" || arg == "--days") {
            if (i + 1 < argc) {
                opts.days = std::stoi(argv[++i]);
            }
        }
        else if (arg == "-n" || arg == "--name") {
            if (i + 1 < argc) {
                opts.commonName = argv[++i];
            }
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg[0] != '-') {
            opts.target = arg;
        }
    }
    
    return opts;
}

// ============================================================================
// Certificate Functions
// ============================================================================

bool readCertificate(const fs::path& path, CertInfo& info) {
#ifdef __linux__
    FILE* fp = fopen(path.string().c_str(), "r");
    if (!fp) return false;
    
    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);
    
    if (!cert) return false;
    
    // Get subject
    char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
    if (subject) {
        info.subject = subject;
        OPENSSL_free(subject);
    }
    
    // Get issuer
    char* issuer = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
    if (issuer) {
        info.issuer = issuer;
        OPENSSL_free(issuer);
    }
    
    // Get serial
    ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    if (serial) {
        info.serial = "";
        for (int i = 0; i < serial->length; i++) {
            char buf[4];
            snprintf(buf, sizeof(buf), "%02x", serial->data[i]);
            info.serial += buf;
            if (i < serial->length - 1) info.serial += ":";
        }
    }
    
    // Get validity dates
    // Convert ASN1_TIME to time_t
    ASN1_TIME* notBeforeASN1 = X509_get_notBefore(cert);
    ASN1_TIME* notAfterASN1 = X509_get_notAfter(cert);
    
    // Simple conversion - parse the ASN1_TIME string format
    if (notBeforeASN1 && notAfterASN1) {
        // Use a simpler approach - convert to tm then to time_t
        std::tm tmBefore = {}, tmAfter = {};
        
        if (notBeforeASN1->type == V_ASN1_UTCTIME) {
            // UTCTIME format: YYMMDDHHMMSSZ
            char* data = (char*)notBeforeASN1->data;
            int len = notBeforeASN1->length;
            if (len >= 10) {
                tmBefore.tm_year = (data[0] - '0') * 10 + (data[1] - '0');
                if (tmBefore.tm_year < 50) tmBefore.tm_year += 100;
                tmBefore.tm_mon = (data[2] - '0') * 10 + (data[3] - '0') - 1;
                tmBefore.tm_mday = (data[4] - '0') * 10 + (data[5] - '0');
                tmBefore.tm_hour = (data[6] - '0') * 10 + (data[7] - '0');
                tmBefore.tm_min = (data[8] - '0') * 10 + (data[9] - '0');
                if (len >= 12) {
                    tmBefore.tm_sec = (data[10] - '0') * 10 + (data[11] - '0');
                }
            }
            tmBefore.tm_isdst = 0;
            info.notBefore = std::chrono::system_clock::from_time_t(std::mktime(&tmBefore));
        }
        
        if (notAfterASN1->type == V_ASN1_UTCTIME) {
            char* data = (char*)notAfterASN1->data;
            int len = notAfterASN1->length;
            if (len >= 10) {
                tmAfter.tm_year = (data[0] - '0') * 10 + (data[1] - '0');
                if (tmAfter.tm_year < 50) tmAfter.tm_year += 100;
                tmAfter.tm_mon = (data[2] - '0') * 10 + (data[3] - '0') - 1;
                tmAfter.tm_mday = (data[4] - '0') * 10 + (data[5] - '0');
                tmAfter.tm_hour = (data[6] - '0') * 10 + (data[7] - '0');
                tmAfter.tm_min = (data[8] - '0') * 10 + (data[9] - '0');
                if (len >= 12) {
                    tmAfter.tm_sec = (data[10] - '0') * 10 + (data[11] - '0');
                }
            }
            tmAfter.tm_isdst = 0;
            info.notAfter = std::chrono::system_clock::from_time_t(std::mktime(&tmAfter));
        }
    }
    
    // Get common name from subject
    X509_NAME* name = X509_get_subject_name(cert);
    if (name) {
        char buf[256];
        if (X509_NAME_get_text_by_NID(name, NID_commonName, buf, sizeof(buf)) > 0) {
            info.commonName = buf;
        }
    }
    
    // Check validity
    auto now = std::chrono::system_clock::now();
    info.isExpired = info.notAfter < now;
    info.isValid = !info.isExpired;
    
    auto daysUntil = std::chrono::duration_cast<std::chrono::days>(info.notAfter - now);
    info.daysUntilExpiry = static_cast<int>(daysUntil.count());
    
    X509_free(cert);
    return true;
#else
    return false;
#endif
}

std::string formatExpiry(const CertInfo& info) {
    if (info.isExpired) {
        return "EXPIRED";
    }
    
    if (info.daysUntilExpiry <= 0) {
        return "TODAY";
    }
    
    if (info.daysUntilExpiry <= 30) {
        return std::to_string(info.daysUntilExpiry) + "d";
    }
    
    auto time = std::chrono::system_clock::to_time_t(info.notAfter);
    std::tm tm = *std::localtime(&time);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}
