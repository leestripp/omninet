// omninet-db - Database Management Tool for OmniNet
// Query, inspect, backup, and maintain the SQLite database

#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <chrono>

#include "tools_common/common.h"
#include "tools_common/colors.h"
#include "tools_common/table.h"
#include "tools_common/database.h"

using namespace OmniNet::Tools;

namespace fs = std::filesystem;

// ============================================================================
// Configuration
// ============================================================================

struct Options {
    std::string action;            // info, tables, query, vacuum, backup, restore
    std::string query;
    std::string file;
    bool json = false;
    bool verbose = false;
};

Options parseArgs(int argc, char* argv[]);

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    auto opts = parseArgs(argc, argv);
    
    bool useColors = isatty(STDOUT_FILENO);
    Formatter out(std::cout, useColors);
    
    // Get database path
    auto dbPath = Paths::databaseFile();
    
    // Open database
    Database db;
    if (!db.open(dbPath.string())) {
        std::cerr << "Error: Cannot open database: " << db.lastError() << '\n';
        return 1;
    }
    
    // Execute action
    if (opts.action == "info") {
        out.header("Database Information");
        out.kv("Path", db.path());
        
        // File size
        if (fs::exists(db.path())) {
            auto size = fs::file_size(db.path());
            double sizeKb = static_cast<double>(size) / 1024.0;
            out.kv("Size", sizeKb < 1024 ? 
                   std::to_string(static_cast<int>(sizeKb)) + " KB" :
                   std::to_string(static_cast<int>(sizeKb / 1024)) + " MB");
        }
        
        // Tables
        auto tables = db.listTables();
        out.kv("Tables", tables.size());
        
        if (opts.verbose) {
            std::cout << '\n';
            out.text("Tables:", Style::KEY);
            for (const auto& table : tables) {
                out.text("  " + table, Style::NORMAL);
            }
        }
        
    } else if (opts.action == "tables") {
        auto tables = db.listTables();
        
        if (tables.empty()) {
            out.info("No tables found");
            return 0;
        }
        
        Table table;
        table.addColumn("Table Name", 30);
        table.addColumn("Columns", 10);
        
        for (const auto& tableName : tables) {
            auto columns = db.tableColumns(tableName);
            table.addRow({
                tableName,
                std::to_string(columns.size())
            });
        }
        
        table.render(std::cout);
        
    } else if (opts.action == "schema") {
        if (opts.file.empty()) {
            std::cerr << "Error: Table name required\n";
            return 1;
        }
        
        auto columns = db.tableColumns(opts.file);
        
        out.header("Schema: " + opts.file);
        
        Table table;
        table.addColumn("Column", 25);
        table.addColumn("Type", 15);
        table.addColumn("Nullable", 10);
        table.addColumn("Default", 15);
        table.addColumn("PK", 4);
        
        for (const auto& col : columns) {
            table.addRow({
                col.name,
                col.type,
                col.notNull ? "NO" : "YES",
                col.defaultValue.empty() ? "-" : col.defaultValue,
                col.primaryKey ? "YES" : "NO"
            });
        }
        
        table.render(std::cout);
        
    } else if (opts.action == "query") {
        if (opts.query.empty()) {
            std::cerr << "Error: Query required\n";
            std::cerr << "Usage: omninet-db query \"SELECT * FROM users\"\n";
            return 1;
        }
        
        // Execute query
        std::vector<std::vector<std::string>> results;
        bool hasResults = false;
        
        if (opts.query.find("SELECT") != std::string::npos ||
            opts.query.find("select") != std::string::npos) {
            // SELECT query - show results
            db.query(opts.query, [&results, &hasResults](const std::vector<std::string>& row) {
                if (!hasResults) hasResults = true;
                results.push_back(row);
                return true;
            });
            
            if (!hasResults || results.empty()) {
                out.info("Query returned no results");
                return 0;
            }
            
            // Display results
            for (const auto& row : results) {
                for (size_t i = 0; i < row.size(); ++i) {
                    std::cout << row[i];
                    if (i < row.size() - 1) std::cout << " | ";
                }
                std::cout << '\n';
            }
            
            out.kv("Rows returned", results.size());
            
        } else {
            // Other query - just execute
            if (db.execute(opts.query)) {
                out.success("Query executed");
                out.kv("Affected rows", db.affectedRows());
            } else {
                out.error("Query failed: " + db.lastError());
                return 1;
            }
        }
        
    } else if (opts.action == "vacuum") {
        out.info("Running VACUUM...");
        
        if (db.vacuum()) {
            out.success("Database vacuumed successfully");
        } else {
            out.error("Vacuum failed: " + db.lastError());
            return 1;
        }
        
    } else if (opts.action == "analyze") {
        out.info("Running ANALYZE...");
        
        if (db.analyze()) {
            out.success("Database analyzed successfully");
        } else {
            out.error("Analyze failed: " + db.lastError());
            return 1;
        }
        
    } else if (opts.action == "backup") {
        if (opts.file.empty()) {
            // Default backup name with timestamp
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&time);
            char buf[64];
            strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm);
            opts.file = "omninet_backup_" + std::string(buf) + ".db";
        }
        
        out.info("Backing up database to: " + opts.file);
        
        if (db.backup(opts.file)) {
            out.success("Backup created successfully");
            
            if (fs::exists(opts.file)) {
                auto size = fs::file_size(opts.file);
                double sizeKb = static_cast<double>(size) / 1024.0;
                out.kv("Size", sizeKb < 1024 ? 
                       std::to_string(static_cast<int>(sizeKb)) + " KB" :
                       std::to_string(static_cast<int>(sizeKb / 1024)) + " MB");
            }
        } else {
            out.error("Backup failed");
            return 1;
        }
        
    } else if (opts.action == "restore") {
        if (opts.file.empty()) {
            std::cerr << "Error: Backup file required\n";
            std::cerr << "Usage: omninet-db restore <backup-file>\n";
            return 1;
        }
        
        if (!fs::exists(opts.file)) {
            std::cerr << "Error: Backup file not found: " << opts.file << '\n';
            return 1;
        }
        
        out.warning("This will replace the current database!");
        std::cout << "Confirm? [y/N] ";
        char confirm;
        std::cin >> confirm;
        
        if (confirm == 'y' || confirm == 'Y') {
            out.info("Restoring database from: " + opts.file);
            
            if (db.restore(opts.file)) {
                out.success("Database restored successfully");
            } else {
                out.error("Restore failed");
                return 1;
            }
        } else {
            out.info("Operation cancelled");
        }
        
    } else {
        // Default: show help
        std::cout << "Usage: omninet-db <command> [options]\n";
        std::cout << '\n';
        std::cout << "Database management for OmniNet.\n";
        std::cout << '\n';
        std::cout << "Commands:\n";
        std::cout << "  info                  Show database information\n";
        std::cout << "  tables                List all tables\n";
        std::cout << "  schema <table>        Show table schema\n";
        std::cout << "  query <sql>           Execute SQL query\n";
        std::cout << "  vacuum                Optimize database\n";
        std::cout << "  analyze               Analyze database\n";
        std::cout << "  backup [file]         Create backup\n";
        std::cout << "  restore <file>        Restore from backup\n";
        std::cout << '\n';
        std::cout << "Options:\n";
        std::cout << "  -h, --help           Show this help\n";
        std::cout << "  -j, --json           Output as JSON\n";
        std::cout << "  -v, --verbose        Verbose output\n";
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
            std::cout << "Usage: omninet-db <command> [options]\n";
            std::cout << '\n';
            std::cout << "Database management for OmniNet.\n";
            std::cout << '\n';
            std::cout << "Commands:\n";
            std::cout << "  info                   Show database information\n";
            std::cout << "  tables                 List all tables\n";
            std::cout << "  schema <table>        Show table schema\n";
            std::cout << "  query <sql>           Execute SQL query\n";
            std::cout << "  vacuum                 Optimize database\n";
            std::cout << "  analyze                Analyze database\n";
            std::cout << "  backup [file]         Create backup\n";
            std::cout << "  restore <file>        Restore from backup\n";
            std::cout << '\n';
            std::cout << "Options:\n";
            std::cout << "  -h, --help            Show this help\n";
            std::cout << "  -j, --json            Output as JSON\n";
            std::cout << "  -v, --verbose         Verbose output\n";
            exit(0);
        }
        else if (arg == "info" || arg == "tables" || arg == "schema" ||
                 arg == "query" || arg == "vacuum" || arg == "analyze" ||
                 arg == "backup" || arg == "restore") {
            opts.action = arg;
        }
        else if (arg == "-j" || arg == "--json") {
            opts.json = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg[0] != '-') {
            if (opts.action == "query") {
                opts.query = arg;
            } else if (opts.action == "schema" || opts.action == "restore") {
                opts.file = arg;
            } else {
                opts.file = arg;
            }
        }
    }
    
    return opts;
}
