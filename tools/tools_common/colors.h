// OmniNet Tools - Terminal Colors and Formatting
#ifndef OMNINET_TOOLS_COLORS_H
#define OMNINET_TOOLS_COLORS_H

#include <string>
#include <ostream>

namespace OmniNet::Tools {

// ============================================================================
// ANSI Color Codes
// ============================================================================

namespace Color {
    // Reset
    constexpr const char* RESET = "\033[0m";
    constexpr const char* BOLD = "\033[1m";
    constexpr const char* DIM = "\033[2m";
    constexpr const char* ITALIC = "\033[3m";
    constexpr const char* UNDERLINE = "\033[4m";
    
    // Foreground colors
    constexpr const char* BLACK = "\033[30m";
    constexpr const char* RED = "\033[31m";
    constexpr const char* GREEN = "\033[32m";
    constexpr const char* YELLOW = "\033[33m";
    constexpr const char* BLUE = "\033[34m";
    constexpr const char* MAGENTA = "\033[35m";
    constexpr const char* CYAN = "\033[36m";
    constexpr const char* WHITE = "\033[37m";
    
    // Bright foreground colors
    constexpr const char* BRIGHT_BLACK = "\033[90m";
    constexpr const char* BRIGHT_RED = "\033[91m";
    constexpr const char* BRIGHT_GREEN = "\033[92m";
    constexpr const char* BRIGHT_YELLOW = "\033[93m";
    constexpr const char* BRIGHT_BLUE = "\033[94m";
    constexpr const char* BRIGHT_MAGENTA = "\033[95m";
    constexpr const char* BRIGHT_CYAN = "\033[96m";
    constexpr const char* BRIGHT_WHITE = "\033[97m";
    
    // Background colors
    constexpr const char* BG_BLACK = "\033[40m";
    constexpr const char* BG_RED = "\033[41m";
    constexpr const char* BG_GREEN = "\033[42m";
    constexpr const char* BG_YELLOW = "\033[43m";
    constexpr const char* BG_BLUE = "\033[44m";
    constexpr const char* BG_MAGENTA = "\033[45m";
    constexpr const char* BG_CYAN = "\033[46m";
    constexpr const char* BG_WHITE = "\033[47m";
}

// ============================================================================
// Text Styles
// ============================================================================

struct Text {
    std::string color;
    bool bold = false;
    bool dim = false;
    
    std::string apply(const std::string& text) const;
};

// Predefined styles
namespace Style {
    constexpr Text NORMAL = {Color::WHITE, false, false};
    constexpr Text BOLD_WHITE = {Color::WHITE, true, false};
    constexpr Text DIM_WHITE = {Color::WHITE, false, true};
    
    constexpr Text SUCCESS = {Color::GREEN, true, false};
    constexpr Text ERROR = {Color::RED, true, false};
    constexpr Text WARNING = {Color::YELLOW, true, false};
    constexpr Text INFO = {Color::CYAN, true, false};
    constexpr Text DEBUG = {Color::MAGENTA, false, false};
    
    constexpr Text HEADER = {Color::BRIGHT_CYAN, true, false};
    constexpr Text SUBHEADER = {Color::BRIGHT_BLUE, true, false};
    constexpr Text KEY = {Color::BRIGHT_YELLOW, false, false};
    constexpr Text VALUE = {Color::BRIGHT_WHITE, false, false};
    
    constexpr Text IP = {Color::BRIGHT_RED, false, false};
    constexpr Text PORT = {Color::BRIGHT_GREEN, false, false};
    constexpr Text PATH = {Color::BRIGHT_MAGENTA, false, false};
}

// ============================================================================
// Formatted Output
// ============================================================================

class Formatter {
public:
    Formatter(std::ostream& out, bool useColors = true);
    
    // Setters
    Formatter& colors(bool enabled);
    Formatter& indent(size_t spaces);
    
    // Output methods
    Formatter& operator()(const std::string& text);
    Formatter& text(const std::string& text);
    Formatter& text(const std::string& text, const Text& style);
    Formatter& line(const std::string& text = "");
    Formatter& line(const std::string& text, const Text& style);
    
    // Convenience methods for styled output
    Formatter& success(const std::string& text);
    Formatter& error(const std::string& text);
    Formatter& warning(const std::string& text);
    Formatter& info(const std::string& text);
    Formatter& debug(const std::string& text);
    
    // Key-value pairs
    Formatter& kv(const std::string& key, const std::string& value);
    Formatter& kv(const std::string& key, int64_t value);
    Formatter& kv(const std::string& key, int value);
    Formatter& kv(const std::string& key, bool value);
    Formatter& kv(const std::string& key, size_t value);
    
    // Section header
    Formatter& header(const std::string& title);
    Formatter& subheader(const std::string& title);
    
    // Separator line
    Formatter& separator(char c = '-', size_t width = 0);
    
    // End line
    void flush();
    
private:
    std::ostream& m_out;
    bool m_useColors = true;
    size_t m_indent = 0;
    std::string m_currentIndent;
};

} // namespace OmniNet::Tools

#endif // OMNINET_TOOLS_COLORS_H
