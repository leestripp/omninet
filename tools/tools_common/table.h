// OmniNet Tools - Table Formatting
#ifndef OMNINET_TOOLS_TABLE_H
#define OMNINET_TOOLS_TABLE_H

#include <string>
#include <vector>
#include <ostream>
#include <optional>

namespace OmniNet::Tools {

// ============================================================================
// Table Column Definition
// ============================================================================

struct Column {
    std::string header;
    size_t width = 0;           // 0 = auto
    enum class Align { LEFT, RIGHT, CENTER } align = Align::LEFT;
    bool truncate = true;       // Truncate if too wide
    
    Column() = default;
    Column(const std::string& h, size_t w = 0, Align a = Align::LEFT)
        : header(h), width(w), align(a) {}
};

// ============================================================================
// Table
// ============================================================================

class Table {
public:
    // Configure
    Table& addColumn(const Column& col);
    Table& addColumn(const std::string& header, size_t width = 0);
    Table& borders(bool enabled);
    Table& headerVisible(bool visible);
    Table& colors(bool enabled);
    
    // Add rows
    Table& addRow(const std::vector<std::string>& cells);
    template<typename... Args>
    Table& addRow(const std::string& first, Args&&... args);
    
    // Rendering
    void render(std::ostream& out);
    std::string toString();
    
    // Clear
    void clear();

private:
    std::vector<Column> m_columns;
    std::vector<std::vector<std::string>> m_rows;
    bool m_borders = true;
    bool m_headerVisible = true;
    bool m_useColors = true;
    
    void renderHeader(std::ostream& out);
    void renderRow(std::ostream& out, const std::vector<std::string>& row);
    std::string pad(const std::string& text, size_t width, Column::Align align);
    std::string truncateText(const std::string& text, size_t maxWidth);
    size_t calculateTotalWidth();
};

// ============================================================================
// Key-Value List (alternative to table)
// ============================================================================

class KeyValueList {
public:
    KeyValueList& add(const std::string& key, const std::string& value);
    KeyValueList& add(const std::string& key, int64_t value);
    KeyValueList& add(const std::string& key, bool value);
    KeyValueList& add(const std::string& key, const std::string& value, const std::string& unit);
    KeyValueList& separator(const std::string& label = "");
    
    void render(std::ostream& out);
    std::string toString();
    
    void clear();

private:
    struct Entry {
        std::string key;
        std::string value;
        std::string unit;
        bool isSeparator = false;
        std::string separatorLabel;
    };
    std::vector<Entry> m_entries;
    bool m_useColors = true;
};

// ============================================================================
// Progress Bar
// ============================================================================

class ProgressBar {
public:
    ProgressBar(size_t total, size_t width = 40);
    
    ProgressBar& update(size_t current);
    ProgressBar& setPrefix(const std::string& prefix);
    ProgressBar& setSuffix(const std::string& suffix);
    ProgressBar& colors(bool enabled);
    
    void render(std::ostream& out);
    void finish(std::ostream& out);

private:
    size_t m_total;
    size_t m_current = 0;
    size_t m_width;
    std::string m_prefix;
    std::string m_suffix;
    bool m_useColors = true;
};

// ============================================================================
// Status Indicators
// ============================================================================

namespace Status {
    constexpr const char* OK = "[OK]";
    constexpr const char* ERROR = "[ERROR]";
    constexpr const char* WARNING = "[WARN]";
    constexpr const char* INFO = "[INFO]";
    constexpr const char* RUNNING = "[RUNNING]";
    constexpr const char* STOPPED = "[STOPPED]";
    constexpr const char* PENDING = "[PENDING]";
    
    // Colored versions (use with terminal)
    constexpr const char* OK_COLOR = "\033[32m[OK]\033[0m";
    constexpr const char* ERROR_COLOR = "\033[31m[ERROR]\033[0m";
    constexpr const char* WARNING_COLOR = "\033[33m[WARN]\033[0m";
    constexpr const char* INFO_COLOR = "\033[36m[INFO]\033[0m";
    constexpr const char* RUNNING_COLOR = "\033[32m[RUNNING]\033[0m";
    constexpr const char* STOPPED_COLOR = "\033[31m[STOPPED]\033[0m";
    constexpr const char* PENDING_COLOR = "\033[33m[PENDING]\033[0m";
}

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_TABLE_H