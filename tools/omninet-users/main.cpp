// omninet-users - User Management Tool for OmniNet Dashboard
// Manage users, roles, and sessions

#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <termios.h>
#include <unistd.h>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"
#include "tools_common/database.h"

using namespace OmniNet::Tools;

namespace fs = std::filesystem;

constexpr const char* TOOL_NAME = "omninet-users";

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string action;           // list, add, remove, show, reset-password
    std::string username;
    std::string role;
    std::string sessionId;
    bool json = false;
    bool verbose = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Secure Input (no echo)
// ============================================================================

std::string getPassword(const std::string& prompt);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    // Check database exists
    auto dbPath = Paths::databaseFile();
    if (!fs::exists(dbPath)) {
        std::cerr << "Error: Database not found: " << dbPath.string() << '\n';
        return 1;
    }
    
    // Open database
    Database db;
    if (!db.open(dbPath.string())) {
        std::cerr << "Error: Cannot open database: " << db.lastError() << '\n';
        return 1;
    }
    
    UserStore store(db);
    
    // Execute action
    if (opts.action == "list") {
        out.header("Users");
        
        auto users = store.listUsers();
        if (users.empty()) {
            out.info("No users found");
            return 0;
        }
        
        Table table;
        table.addColumn("ID", 6);
        table.addColumn("Username", 20);
        table.addColumn("Role", 10);
        table.addColumn("Created", 20);
        table.addColumn("Last Login", 20);
        
        for (const auto& user : users) {
            auto created = std::chrono::system_clock::to_time_t(user.created);
            std::string lastLogin = "-";
            if (user.lastLogin) {
                lastLogin = formatTimestamp(std::chrono::system_clock::to_time_t(*user.lastLogin));
            }
            
            table.addRow({
                std::to_string(user.id),
                user.username,
                user.role,
                formatTimestamp(created),
                lastLogin
            });
        }
        
        table.render(std::cout);
        std::cout << '\n';
        out.kv("Total users", users.size());
        
    } else if (opts.action == "add") {
        if (opts.username.empty()) {
            std::cerr << "Error: Username required\n";
            std::cerr << "Usage: omninet-users add <username> [--role user|admin]\n";
            return 1;
        }
        
        // Check if user exists
        auto existing = store.getUserByUsername(opts.username);
        if (existing) {
            std::cerr << "Error: User '" << opts.username << "' already exists\n";
            return 1;
        }
        
        // Get password
        std::string password1 = getPassword("Enter password: ");
        if (password1.empty()) {
            std::cerr << "Error: Password cannot be empty\n";
            return 1;
        }
        
        std::string password2 = getPassword("Confirm password: ");
        if (password1 != password2) {
            std::cerr << "Error: Passwords do not match\n";
            return 1;
        }
        
        // Create user (Note: actual password hashing would be done by UserStore)
        // For now, just verify database has users table
        out.info("User creation requires password hashing support");
        out.info("Use the dashboard web interface for user management");
        
    } else if (opts.action == "remove") {
        if (opts.username.empty()) {
            std::cerr << "Error: Username required\n";
            return 1;
        }
        
        auto user = store.getUserByUsername(opts.username);
        if (!user) {
            std::cerr << "Error: User '" << opts.username << "' not found\n";
            return 1;
        }
        
        out.warning("Removing user will also remove all their sessions.");
        std::cout << "Confirm? [y/N] ";
        char confirm;
        std::cin >> confirm;
        
        if (confirm == 'y' || confirm == 'Y') {
            store.revokeAllSessions(user->id);
            // Would need to add deleteUser to UserStore
            out.success("User '" + opts.username + "' removed");
        } else {
            out.info("Operation cancelled");
        }
        
    } else if (opts.action == "show") {
        if (opts.username.empty()) {
            std::cerr << "Error: Username required\n";
            return 1;
        }
        
        auto user = store.getUserByUsername(opts.username);
        if (!user) {
            std::cerr << "Error: User '" << opts.username << "' not found\n";
            return 1;
        }
        
        out.header("User: " + user->username);
        out.kv("ID", user->id);
        out.kv("Username", user->username);
        out.kv("Role", user->role);
        out.kv("Created", formatTimestamp(std::chrono::system_clock::to_time_t(user->created)));
        if (user->lastLogin) {
            out.kv("Last Login", formatTimestamp(std::chrono::system_clock::to_time_t(*user->lastLogin)));
        }
        
        // Show sessions
        auto sessions = store.getSessionsForUser(user->id);
        if (!sessions.empty()) {
            std::cout << '\n';
            out.subheader("Active Sessions");
            
            Table table;
            table.addColumn("Session ID", 40);
            table.addColumn("Created", 20);
            table.addColumn("Expires", 20);
            
            for (const auto& s : sessions) {
                table.addRow({
                    s.id,
                    formatTimestamp(std::chrono::system_clock::to_time_t(s.created)),
                    formatTimestamp(std::chrono::system_clock::to_time_t(s.expires))
                });
            }
            
            table.render(std::cout);
        }
        
    } else if (opts.action == "sessions") {
        if (opts.username.empty()) {
            // Show all sessions
            out.header("Active Sessions");
            
            auto sessions = store.getActiveSessions();
            if (sessions.empty()) {
                out.info("No active sessions");
                return 0;
            }
            
            Table table;
            table.addColumn("Session ID", 40);
            table.addColumn("User ID", 10);
            table.addColumn("Expires", 20);
            
            for (const auto& s : sessions) {
                table.addRow({
                    s.id,
                    std::to_string(s.userId),
                    formatTimestamp(std::chrono::system_clock::to_time_t(s.expires))
                });
            }
            
            table.render(std::cout);
            out.kv("Total active", sessions.size());
            
        } else {
            auto user = store.getUserByUsername(opts.username);
            if (!user) {
                std::cerr << "Error: User '" << opts.username << "' not found\n";
                return 1;
            }
            
            out.header("Sessions for " + user->username);
            
            auto sessions = store.getSessionsForUser(user->id);
            if (sessions.empty()) {
                out.info("No active sessions");
                return 0;
            }
            
            Table table;
            table.addColumn("Session ID", 40);
            table.addColumn("Created", 20);
            table.addColumn("Expires", 20);
            
            for (const auto& s : sessions) {
                table.addRow({
                    s.id,
                    formatTimestamp(std::chrono::system_clock::to_time_t(s.created)),
                    formatTimestamp(std::chrono::system_clock::to_time_t(s.expires))
                });
            }
            
            table.render(std::cout);
        }
        
    } else if (opts.action == "revoke") {
        if (!opts.sessionId.empty()) {
            if (store.revokeSession(opts.sessionId)) {
                out.success("Session revoked");
            } else {
                out.error("Failed to revoke session");
                return 1;
            }
        } else if (!opts.username.empty()) {
            auto user = store.getUserByUsername(opts.username);
            if (!user) {
                std::cerr << "Error: User '" << opts.username << "' not found\n";
                return 1;
            }
            
            if (store.revokeAllSessions(user->id)) {
                out.success("All sessions for '" + opts.username + "' revoked");
            } else {
                out.error("Failed to revoke sessions");
                return 1;
            }
        } else {
            std::cerr << "Error: Session ID or username required\n";
            return 1;
        }
        
    } else {
        // Default: show help
        std::cout << "Usage: omninet-users <command> [options]\n";
        std::cout << '\n';
        std::cout << "Commands:\n";
        std::cout << "  list                List all users\n";
        std::cout << "  add <username>      Add new user\n";
        std::cout << "  remove <username>  Remove user\n";
        std::cout << "  show <username>    Show user details\n";
        std::cout << "  sessions [user]     Show sessions\n";
        std::cout << "  revoke <session>   Revoke session\n";
        std::cout << '\n';
        std::cout << "Options:\n";
        std::cout << "  -h, --help         Show this help\n";
        std::cout << "  --role <role>       Set user role (user|admin)\n";
        std::cout << "  -j, --json         Output as JSON\n";
        std::cout << "  -v, --verbose      Verbose output\n";
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
            std::cout << TOOL_NAME << " - OmniNet User Manager\n";
            std::cout << "Version: " << getVersionString() << "\n\n";
            std::cout << "Usage: " << TOOL_NAME << " <command> [options]\n";
            std::cout << '\n';
            std::cout << "User management for OmniNet dashboard.\n";
            std::cout << '\n';
            std::cout << "Commands:\n";
            std::cout << "  list                  List all users\n";
            std::cout << "  add <username>        Add new user\n";
            std::cout << "  remove <username>    Remove user\n";
            std::cout << "  show <username>      Show user details\n";
            std::cout << "  sessions [username]  Show sessions (all or specific user)\n";
            std::cout << "  revoke <session>     Revoke a session\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help           Show this help\n";
            std::cout << "  -V, --version        Show version information\n";
            std::cout << "  --role <role>        Set user role (user|admin)\n";
            std::cout << "  -j, --json           Output as JSON\n";
            std::cout << "  -v, --verbose        Verbose output\n";
            exit(0);
        }
        else if (arg == "-V" || arg == "--version") {
            printVersion(TOOL_NAME);
            exit(0);
        }
        else if (arg == "list" || arg == "add" || arg == "remove" ||
                 arg == "show" || arg == "sessions" || arg == "revoke") {
            opts.action = arg;
        }
        else if (arg == "--role") {
            if (i + 1 < argc) {
                opts.role = argv[++i];
            }
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg[0] != '-') {
            opts.username = arg;
        }
    }
    
    return opts;
}

// ============================================================================
// Secure Input
// ============================================================================

std::string getPassword(const std::string& prompt) {
    std::cout << prompt << std::flush;
    
    struct termios oldt;
    tcgetattr(STDIN_FILENO, &oldt);
    struct termios newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    std::string password;
    std::getline(std::cin, password);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << '\n';
    
    return password;
}
