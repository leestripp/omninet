// omninet-ctl - Server Process Manager
// Start, stop, restart, and manage OmniNet server instances

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"
#include "tools_common/config.h"

namespace fs = std::filesystem;
using namespace OmniNet::Tools;

constexpr const char* TOOL_NAME = "omninet-ctl";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string action;            // start, stop, restart, status, list
    std::string serverName;
    std::string configFile;
    std::string executable;
    bool json = false;
    bool verbose = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Process Control
// ============================================================================

class ServerProcess {
public:
    ServerProcess(const std::string& name) : m_name(name) {}
    
    // PID file management
    fs::path pidFile() const;
    
    // Check if running
    bool isRunning() const;
    
    // Get PID
    std::optional<pid_t> getPid() const;
    
    // Start server
    bool start(const std::string& executable, const std::string& configFile = {});
    
    // Stop server
    bool stop(int timeoutSeconds = 10);
    
    // Kill server (force)
    bool kill();
    
    // Get server info
    const std::string& name() const { return m_name; }
    
private:
    std::string m_name;
};

bool ServerProcess::start(const std::string& executable, const std::string& configFile) {
    if (isRunning()) {
        return false; // Already running
    }
    
    // Create PID file
    PidFile pidFileHelper(pidFile());
    
    // Fork process
    pid_t childPid = fork();
    
    if (childPid < 0) {
        std::cerr << "Failed to fork process\n";
        return false;
    }
    
    if (childPid == 0) {
        // Child process
        
        // Redirect output to log file
        auto logPath = Paths::logFile();
        auto lfp = fopen(logPath.string().c_str(), "a");
        if (lfp) {
            dup2(fileno(lfp), STDOUT_FILENO);
            dup2(fileno(lfp), STDERR_FILENO);
        }
        
        // Execute server
        std::vector<char*> args;
        args.push_back(const_cast<char*>(executable.c_str()));
        
        if (!configFile.empty()) {
            args.push_back(const_cast<char*>("--config"));
            args.push_back(const_cast<char*>(configFile.c_str()));
        }
        
        args.push_back(nullptr);
        
        execvp(args[0], args.data());
        
        // If exec fails
        std::cerr << "Failed to execute: " << executable << '\n';
        exit(1);
    }
    
    // Parent process
    pidFileHelper.write();
    return true;
}

bool ServerProcess::stop(int timeoutSeconds) {
    auto procPid = getPid();
    if (!procPid) {
        return false; // Not running
    }
    
    // Send SIGTERM
    if (::kill(*procPid, SIGTERM) != 0) {
        return false;
    }
    
    // Wait for process to exit
    for (int i = 0; i < timeoutSeconds * 10; ++i) {
        if (!isRunning()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

bool ServerProcess::kill() {
    auto procPid = getPid();
    if (!procPid) {
        return false;
    }
    
    if (::kill(*procPid, SIGKILL) == 0) {
        return true;
    }
    
    return false;
}

bool ServerProcess::isRunning() const {
    auto procPid = getPid();
    if (!procPid) {
        return false;
    }
    return PidFile::isRunning(*procPid);
}

std::optional<pid_t> ServerProcess::getPid() const {
    return PidFile::read(pidFile());
}

fs::path ServerProcess::pidFile() const {
    auto safeName = m_name;
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    return Paths::dataDir() / ("omninet-" + safeName + ".pid");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    ServerConfigReader reader;
    
    // Execute action
    if (opts.action == "list") {
        out.header("Configured Servers");
        
        auto servers = reader.listServers();
        
        if (servers.empty()) {
            out.info("No servers configured");
            out.info("Use 'omninet-ctl add <name>' to add a server");
            return 0;
        }
        
        Table table;
        table.addColumn("Name", 20);
        table.addColumn("Address", 15);
        table.addColumn("Port", 8);
        table.addColumn("Status", 12);
        
        for (const auto& srv : servers) {
            ServerProcess proc(srv.name);
            std::string status = proc.isRunning() ? "RUNNING" : "STOPPED";
            
            table.addRow({
                srv.name,
                srv.address,
                std::to_string(srv.port),
                status
            });
        }
        
        table.render(std::cout);
        
    } else if (opts.action == "status") {
        if (opts.serverName.empty()) {
            // Show all servers
            auto servers = reader.listServers();
            
            for (const auto& srv : servers) {
                ServerProcess proc(srv.name);
                bool running = proc.isRunning();
                
                std::cout << srv.name << ": ";
                if (running) {
                    out.text("RUNNING", Style::SUCCESS);
                } else {
                    out.text("STOPPED", Style::WARNING);
                }
            }
        } else {
            ServerProcess proc(opts.serverName);
            
            if (proc.isRunning()) {
                out.success("Server '" + opts.serverName + "' is running");
                auto pid = proc.getPid();
                if (pid) {
                    out.kv("PID", static_cast<int64_t>(*pid));
                }
            } else {
                out.warning("Server '" + opts.serverName + "' is not running");
            }
        }
        
    } else if (opts.action == "start") {
        if (opts.serverName.empty()) {
            std::cerr << "Error: Server name required\n";
            std::cerr << "Usage: omninet-ctl start <server-name>\n";
            return 1;
        }
        
        auto serverConfig = reader.loadServer(opts.serverName);
        
        ServerProcess proc(opts.serverName);
        
        if (proc.isRunning()) {
            out.warning("Server '" + opts.serverName + "' is already running");
            return 0;
        }
        
        std::string executable = serverConfig ? serverConfig->executable : "./build/demos/http_server_demo";
        
        if (proc.start(executable, opts.configFile)) {
            out.success("Server '" + opts.serverName + "' started");
            
            // Wait a moment and verify
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (proc.isRunning()) {
                out.info("Server is running");
            } else {
                out.warning("Server may have failed to start - check logs");
            }
        } else {
            out.error("Failed to start server");
            return 1;
        }
        
    } else if (opts.action == "stop") {
        if (opts.serverName.empty()) {
            std::cerr << "Error: Server name required\n";
            std::cerr << "Usage: omninet-ctl stop <server-name>\n";
            return 1;
        }
        
        ServerProcess proc(opts.serverName);
        
        if (!proc.isRunning()) {
            out.warning("Server '" + opts.serverName + "' is not running");
            return 0;
        }
        
        out.info("Stopping server...");
        
        if (proc.stop(10)) {
            out.success("Server stopped");
        } else {
            out.warning("Graceful stop failed, killing...");
            if (proc.kill()) {
                out.success("Server killed");
            } else {
                out.error("Failed to stop server");
                return 1;
            }
        }
        
    } else if (opts.action == "restart") {
        if (opts.serverName.empty()) {
            std::cerr << "Error: Server name required\n";
            std::cerr << "Usage: omninet-ctl restart <server-name>\n";
            return 1;
        }
        
        ServerProcess proc(opts.serverName);
        
        out.info("Restarting server...");
        
        proc.stop(10);
        proc.kill(); // Force kill if still running
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto serverConfig = reader.loadServer(opts.serverName);
        std::string executable = serverConfig ? serverConfig->executable : "./build/demos/http_server_demo";
        
        if (proc.start(executable, opts.configFile)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (proc.isRunning()) {
                out.success("Server restarted");
            } else {
                out.warning("Server may have failed to start");
            }
        } else {
            out.error("Failed to restart server");
            return 1;
        }
        
    } else {
        // Default: show help
        std::cout << "Usage: omninet-ctl <command> [options]\n";
        std::cout << '\n';
        std::cout << "Server process management for OmniNet.\n";
        std::cout << '\n';
        std::cout << "Commands:\n";
        std::cout << "  list                  List all servers\n";
        std::cout << "  status [name]        Show server status\n";
        std::cout << "  start <name>         Start server\n";
        std::cout << "  stop <name>          Stop server\n";
        std::cout << "  restart <name>       Restart server\n";
        std::cout << "  add <name>           Add new server config\n";
        std::cout << "  remove <name>        Remove server config\n";
        std::cout << '\n';
        std::cout << "Options:\n";
        std::cout << "  -h, --help          Show this help\n";
        std::cout << "  -c, --config        Config file path\n";
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
            std::cout << TOOL_NAME << " - OmniNet Server Controller\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " <command> [options]\n";
            std::cout << '\n';
            std::cout << "Server process management for OmniNet.\n";
            std::cout << '\n';
            std::cout << "Commands:\n";
            std::cout << "  list                   List all servers\n";
            std::cout << "  status [name]         Show server status\n";
            std::cout << "  start <name>          Start server\n";
            std::cout << "  stop <name>           Stop server\n";
            std::cout << "  restart <name>        Restart server\n";
            std::cout << "  add <name>            Add new server config\n";
            std::cout << "  remove <name>         Remove server config\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help           Show this help\n";
            std::cout << "  -V, --version        Show version information\n";
            std::cout << "  -c, --config         Config file path\n";
            std::cout << "  -j, --json           Output as JSON\n";
            std::cout << "  -v, --verbose        Verbose output\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "list" || arg == "status" ||
                 arg == "start" || arg == "stop" ||
                 arg == "restart" || arg == "add" || arg == "remove") {
            opts.action = arg;
        }
        else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                opts.configFile = argv[++i];
            }
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg[0] != '-') {
            opts.serverName = arg;
        }
    }
    
    return opts;
}
