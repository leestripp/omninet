// OmniNet Tools - Terminal Colors Implementation
#include "colors.h"

#include <iostream>
#include <iomanip>

namespace OmniNet::Tools {

// ============================================================================
// Text Implementation
// ============================================================================

std::string Text::apply(const std::string& text) const {
    if (color.empty()) {
        return text;
    }
    std::string result;
    if (bold) result += Color::BOLD;
    if (dim) result += Color::DIM;
    result += color;
    result += text;
    result += Color::RESET;
    return result;
}

// ============================================================================
// Formatter Implementation
// ============================================================================

Formatter::Formatter(std::ostream& out, bool useColors)
    : m_out(out), m_useColors(useColors) {}

Formatter& Formatter::colors(bool enabled) {
    m_useColors = enabled;
    return *this;
}

Formatter& Formatter::indent(size_t spaces) {
    m_indent = spaces;
    m_currentIndent = std::string(spaces, ' ');
    return *this;
}

Formatter& Formatter::operator()(const std::string& text) {
    m_out << text << std::flush;
    return *this;
}

Formatter& Formatter::text(const std::string& text) {
    m_out << m_currentIndent << text << std::flush;
    return *this;
}

Formatter& Formatter::text(const std::string& text, const Text& style) {
    if (m_useColors) {
        m_out << m_currentIndent << style.apply(text) << std::flush;
    } else {
        m_out << m_currentIndent << text << std::flush;
    }
    return *this;
}

Formatter& Formatter::line(const std::string& text) {
    m_out << m_currentIndent << text << '\n';
    return *this;
}

Formatter& Formatter::line(const std::string& text, const Text& style) {
    if (m_useColors) {
        m_out << m_currentIndent << style.apply(text) << '\n';
    } else {
        m_out << m_currentIndent << text << '\n';
    }
    return *this;
}

Formatter& Formatter::success(const std::string& text) {
    return line("[OK] " + text, Style::SUCCESS);
}

Formatter& Formatter::error(const std::string& text) {
    return line("[ERROR] " + text, Style::ERROR);
}

Formatter& Formatter::warning(const std::string& text) {
    return line("[WARN] " + text, Style::WARNING);
}

Formatter& Formatter::info(const std::string& text) {
    return line("[INFO] " + text, Style::INFO);
}

Formatter& Formatter::debug(const std::string& text) {
    return line("[DEBUG] " + text, Style::DEBUG);
}

Formatter& Formatter::kv(const std::string& key, const std::string& value) {
    if (m_useColors) {
        m_out << m_currentIndent << Style::KEY.apply(key) << ": " 
              << Style::VALUE.apply(value) << '\n';
    } else {
        m_out << m_currentIndent << key << ": " << value << '\n';
    }
    return *this;
}

Formatter& Formatter::kv(const std::string& key, int64_t value) {
    return kv(key, std::to_string(value));
}

Formatter& Formatter::kv(const std::string& key, int value) {
    return kv(key, std::to_string(value));
}

Formatter& Formatter::kv(const std::string& key, size_t value) {
    return kv(key, std::to_string(value));
}

Formatter& Formatter::kv(const std::string& key, bool value) {
    return kv(key, std::string(value ? "true" : "false"));
}

Formatter& Formatter::header(const std::string& title) {
    line("");
    if (m_useColors) {
        m_out << m_currentIndent << Style::HEADER.apply(title) << '\n';
        m_out << m_currentIndent << Style::HEADER.apply(std::string(title.length(), '=')) << '\n';
    } else {
        m_out << m_currentIndent << title << '\n';
        m_out << m_currentIndent << std::string(title.length(), '=') << '\n';
    }
    return *this;
}

Formatter& Formatter::subheader(const std::string& title) {
    if (m_useColors) {
        m_out << m_currentIndent << Style::SUBHEADER.apply(title) << '\n';
        m_out << m_currentIndent << Style::SUBHEADER.apply(std::string(title.length(), '-')) << '\n';
    } else {
        m_out << m_currentIndent << title << '\n';
        m_out << m_currentIndent << std::string(title.length(), '-') << '\n';
    }
    return *this;
}

Formatter& Formatter::separator(char c, size_t width) {
    if (width == 0) {
        width = 60;
    }
    m_out << m_currentIndent << std::string(width, c) << '\n';
    return *this;
}

void Formatter::flush() {
    m_out << std::flush;
}

} // namespace OmniNet::Tools

// ============================================================================
// Spinner Implementation - Uses usleep to avoid std:: namespace conflicts
// ============================================================================

#include <thread>
#include <atomic>
#include <chrono>

// Implement outside OmniNet namespace to avoid std:: conflicts
::OmniNet::Tools::Spinner::Spinner(std::ostream& out, bool useColors, const std::string& message)
    : m_out(out), m_useColors(useColors), m_message(message), m_running(true), m_active(true) {
    m_thread = std::thread(&::OmniNet::Tools::Spinner::run, this);
}

::OmniNet::Tools::Spinner::~Spinner() {
    stop();
}

void ::OmniNet::Tools::Spinner::run() {
    static const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    size_t frame = 0;
    
    while (m_active) {
        if (m_useColors) {
            m_out << "\r" << m_message << " " << frames[frame] << " " << std::flush;
        } else {
            m_out << "\r" << m_message << "..." << std::flush;
        }
        frame = (frame + 1) % 10;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ::OmniNet::Tools::Spinner::update(const std::string& message) {
    m_message = message;
}

void ::OmniNet::Tools::Spinner::stop() {
    m_active = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
    // Clear the spinner line
    std::string spaces(m_message.length() + 15, ' ');
    m_out << "\r" << spaces << "\r" << std::flush;
    m_running = false;
}

void ::OmniNet::Tools::Spinner::stopWithSuccess(const std::string& message) {
    stop();
    std::string prefix = m_useColors ? "\033[1;32m[OK]\033[0m " : "[OK] ";
    m_out << prefix << message << "\n";
}

void ::OmniNet::Tools::Spinner::stopWithError(const std::string& message) {
    stop();
    std::string prefix = m_useColors ? "\033[1;31m[ERROR]\033[0m " : "[ERROR] ";
    m_out << prefix << message << "\n";
}
