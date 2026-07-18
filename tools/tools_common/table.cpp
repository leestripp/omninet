// OmniNet Tools - Table Formatting Implementation
#include "table.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace OmniNet::Tools {

// ============================================================================
// Table Implementation
// ============================================================================

Table& Table::addColumn(const Column& col) {
    m_columns.push_back(col);
    return *this;
}

Table& Table::addColumn(const std::string& header, size_t width) {
    m_columns.push_back(Column(header, width));
    return *this;
}

Table& Table::borders(bool enabled) {
    m_borders = enabled;
    return *this;
}

Table& Table::headerVisible(bool visible) {
    m_headerVisible = visible;
    return *this;
}

Table& Table::colors(bool enabled) {
    m_useColors = enabled;
    return *this;
}

Table& Table::addRow(const std::vector<std::string>& cells) {
    m_rows.push_back(cells);
    return *this;
}

template<typename... Args>
Table& Table::addRow(const std::string& first, Args&&... args) {
    std::vector<std::string> row;
    row.push_back(first);
    (row.push_back(std::forward<Args>(args)), ...);
    m_rows.push_back(std::move(row));
    return *this;
}

void Table::clear() {
    m_rows.clear();
}

size_t Table::calculateTotalWidth() {
    size_t total = 0;
    for (const auto& col : m_columns) {
        total += col.width + 3; // padding + separator
    }
    if (m_borders) {
        total += 4; // borders
    }
    return total;
}

std::string Table::pad(const std::string& text, size_t width, Column::Align align) {
    if (text.length() >= width) {
        return truncateText(text, width);
    }
    
    size_t padding = width - text.length();
    switch (align) {
        case Column::Align::RIGHT:
            return std::string(padding, ' ') + text;
        case Column::Align::CENTER:
            return std::string(padding / 2, ' ') + text + std::string(padding - padding / 2, ' ');
        default:
            return text + std::string(padding, ' ');
    }
}

std::string Table::truncateText(const std::string& text, size_t maxWidth) {
    if (text.length() <= maxWidth) {
        return text;
    }
    if (maxWidth <= 3) {
        return text.substr(0, maxWidth);
    }
    return text.substr(0, maxWidth - 3) + "...";
}

void Table::renderHeader(std::ostream& out) {
    if (!m_headerVisible || m_columns.empty()) return;
    
    // Header row
    if (m_borders) {
        out << "| ";
    }
    
    for (size_t i = 0; i < m_columns.size(); ++i) {
        const auto& col = m_columns[i];
        std::string header;
        if (m_useColors) {
            header = "\033[1m" + col.header + "\033[0m";
        } else {
            header = col.header;
        }
        
        out << pad(header, col.width, Column::Align::LEFT);
        if (i < m_columns.size() - 1) {
            out << " | ";
        }
    }
    
    if (m_borders) {
        out << " |";
    }
    out << '\n';
    
    // Separator
    if (m_borders) {
        out << "|";
        for (size_t i = 0; i < m_columns.size(); ++i) {
            out << "-" << std::string(m_columns[i].width, '-') << "-";
            if (i < m_columns.size() - 1) {
                out << "+";
            }
        }
        out << "|" << '\n';
    }
}

void Table::renderRow(std::ostream& out, const std::vector<std::string>& row) {
    if (m_borders) {
        out << "| ";
    }
    
    for (size_t i = 0; i < m_columns.size() && i < row.size(); ++i) {
        const auto& col = m_columns[i];
        out << pad(row[i], col.width, col.align);
        if (i < m_columns.size() - 1) {
            out << " | ";
        }
    }
    // Fill empty cells
    for (size_t i = row.size(); i < m_columns.size(); ++i) {
        out << pad("", m_columns[i].width, m_columns[i].align);
        if (i < m_columns.size() - 1) {
            out << " | ";
        }
    }
    
    if (m_borders) {
        out << " |";
    }
    out << '\n';
}

void Table::render(std::ostream& out) {
    if (m_columns.empty()) return;
    
    renderHeader(out);
    
    for (const auto& row : m_rows) {
        renderRow(out, row);
    }
}

std::string Table::toString() {
    std::ostringstream ss;
    render(ss);
    return ss.str();
}

// ============================================================================
// KeyValueList Implementation
// ============================================================================

KeyValueList& KeyValueList::add(const std::string& key, const std::string& value) {
    m_entries.push_back({key, value, "", false});
    return *this;
}

KeyValueList& KeyValueList::add(const std::string& key, int64_t value) {
    return add(key, std::to_string(value));
}

KeyValueList& KeyValueList::add(const std::string& key, bool value) {
    return add(key, std::string(value ? "true" : "false"));
}

KeyValueList& KeyValueList::add(const std::string& key, const std::string& value, const std::string& unit) {
    m_entries.push_back({key, value, unit, false});
    return *this;
}

KeyValueList& KeyValueList::separator(const std::string& label) {
    m_entries.push_back({"", "", "", true, label});
    return *this;
}

void KeyValueList::clear() {
    m_entries.clear();
}

void KeyValueList::render(std::ostream& out) {
    for (const auto& entry : m_entries) {
        if (entry.isSeparator) {
            if (!entry.separatorLabel.empty()) {
                out << "\n--- " << entry.separatorLabel << " ---\n";
            } else {
                out << '\n';
            }
            continue;
        }
        
        if (m_useColors) {
            out << "  \033[33m" << std::left << std::setw(20) << entry.key << "\033[0m: ";
        } else {
            out << "  " << std::left << std::setw(20) << entry.key << ": ";
        }
        
        out << entry.value;
        if (!entry.unit.empty()) {
            out << " " << entry.unit;
        }
        out << '\n';
    }
}

std::string KeyValueList::toString() {
    std::ostringstream ss;
    render(ss);
    return ss.str();
}

// ============================================================================
// ProgressBar Implementation
// ============================================================================

ProgressBar::ProgressBar(size_t total, size_t width)
    : m_total(total), m_width(width) {}

ProgressBar& ProgressBar::update(size_t current) {
    m_current = current;
    return *this;
}

ProgressBar& ProgressBar::setPrefix(const std::string& prefix) {
    m_prefix = prefix;
    return *this;
}

ProgressBar& ProgressBar::setSuffix(const std::string& suffix) {
    m_suffix = suffix;
    return *this;
}

ProgressBar& ProgressBar::colors(bool enabled) {
    m_useColors = enabled;
    return *this;
}

void ProgressBar::render(std::ostream& out) {
    if (m_total == 0) return;
    
    size_t progress = (m_current * m_width) / m_total;
    size_t percentage = (m_current * 100) / m_total;
    
    out << '\r' << m_prefix;
    
    out << "[";
    for (size_t i = 0; i < m_width; ++i) {
        if (i < progress) {
            if (m_useColors) {
                out << "\033[32m=\033[0m";
            } else {
                out << "=";
            }
        } else {
            out << " ";
        }
    }
    out << "] ";
    
    out << std::setw(3) << percentage << "%";
    
    if (!m_suffix.empty()) {
        out << " " << m_suffix;
    }
    
    out << std::flush;
}

void ProgressBar::finish(std::ostream& out) {
    m_current = m_total;
    render(out);
    out << '\n';
}

} // namespace OmniNet::Tools