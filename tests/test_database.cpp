// OmniNet Database Tests
#include "db/Database.h"
#include <iostream>
#include <filesystem>

using namespace OmniNet::DB;

bool test_open_close() {
    std::string path = "test.db";
    
    // Clean up any existing file
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    
    Database db;
    db.open(path);
    
    if (!db.isOpen()) return false;
    
    db.close();
    
    if (db.isOpen()) return false;
    
    // Clean up
    std::filesystem::remove(path);
    
    return true;
}

bool test_execute() {
    std::string path = "test.db";
    
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    
    Database db;
    db.open(path);
    
    // Create table
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    
    // Insert data
    db.execute("INSERT INTO test (name) VALUES ('Alice')");
    db.execute("INSERT INTO test (name) VALUES ('Bob')");
    
    db.close();
    std::filesystem::remove(path);
    
    return true;
}

bool test_query() {
    std::string path = "test.db";
    
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    
    Database db;
    db.open(path);
    
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    db.execute("INSERT INTO test (name) VALUES ('Alice')");
    db.execute("INSERT INTO test (name) VALUES ('Bob')");
    
    std::vector<std::string> names;
    db.query("SELECT name FROM test", [&names](const std::vector<std::string>& row) {
        names.push_back(row[0]);
        return true;
    });
    
    db.close();
    std::filesystem::remove(path);
    
    if (names.size() != 2) return false;
    if (names[0] != "Alice") return false;
    if (names[1] != "Bob") return false;
    
    return true;
}

bool test_transaction() {
    std::string path = "test.db";
    
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    
    Database db;
    db.open(path);
    
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    
    // Test successful transaction
    db.transaction([&db]() {
        db.execute("INSERT INTO test (name) VALUES ('Transaction1')");
        db.execute("INSERT INTO test (name) VALUES ('Transaction2')");
    });
    
    auto result = db.queryOne("SELECT COUNT(*) FROM test");
    if (!result || result->empty()) return false;
    
    db.close();
    std::filesystem::remove(path);
    
    return true;
}

bool test_transaction_rollback() {
    std::string path = "test.db";
    
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    
    Database db;
    db.open(path);
    
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    db.execute("INSERT INTO test (name) VALUES ('Before')");
    
    // Test rollback
    bool rolledBack = false;
    try {
        db.transaction([&db, &rolledBack]() {
            db.execute("INSERT INTO test (name) VALUES ('InTransaction')");
            rolledBack = true;
            throw std::runtime_error("Test error");
        });
    } catch (...) {
        // Expected
    }
    
    auto result = db.queryOne("SELECT COUNT(*) FROM test");
    
    db.close();
    std::filesystem::remove(path);
    
    if (!rolledBack) return false;
    if (!result || result->empty()) return false;
    
    return true;
}

bool test_last_insert_rowid() {
    std::string path = "test.db";
    
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
    }
    
    Database db;
    db.open(path);
    
    db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT)");
    db.execute("INSERT INTO test (name) VALUES ('First')");
    
    auto id = db.lastInsertRowId();
    
    db.close();
    std::filesystem::remove(path);
    
    return id == 1;
}

int main() {
    std::cout << "Running Database tests..." << std::endl;
    
    int passed = 0;
    int failed = 0;
    
    if (test_open_close()) {
        std::cout << "[PASS] Open/close database" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Open/close database" << std::endl;
        failed++;
    }
    
    if (test_execute()) {
        std::cout << "[PASS] Execute SQL" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Execute SQL" << std::endl;
        failed++;
    }
    
    if (test_query()) {
        std::cout << "[PASS] Query with callback" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Query with callback" << std::endl;
        failed++;
    }
    
    if (test_transaction()) {
        std::cout << "[PASS] Transaction commit" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Transaction commit" << std::endl;
        failed++;
    }
    
    if (test_transaction_rollback()) {
        std::cout << "[PASS] Transaction rollback" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Transaction rollback" << std::endl;
        failed++;
    }
    
    if (test_last_insert_rowid()) {
        std::cout << "[PASS] Last insert row ID" << std::endl;
        passed++;
    } else {
        std::cout << "[FAIL] Last insert row ID" << std::endl;
        failed++;
    }
    
    std::cout << std::endl;
    std::cout << "Database tests: " << passed << " passed, " << failed << " failed" << std::endl;
    
    return failed > 0 ? 1 : 0;
}