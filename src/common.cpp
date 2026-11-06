#include "common.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#endif

namespace css_checker {
namespace utils {

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

std::string to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool ends_with_ignore_case(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) {
        return false;
    }
    for (size_t i = 0; i < suffix.size(); ++i) {
        unsigned char a = static_cast<unsigned char>(str[str.size() - suffix.size() + i]);
        unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

bool is_class_token(const std::string& token) {
    if (token.empty()) {
        return false;
    }

    size_t i = 0;
    unsigned char first = static_cast<unsigned char>(token[0]);
    if (first == '-') {
        if (token.size() < 2) {
            return false;
        }
        unsigned char next = static_cast<unsigned char>(token[1]);
        if (!(std::isalpha(next) || next == '_')) {
            return false;
        }
        i = 2;
    } else if (!(std::isalpha(first) || first == '_')) {
        return false;
    } else {
        i = 1;
    }

    for (; i < token.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(token[i]);
        if (!(std::isalnum(c) || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

std::string get_relative_path(const std::string& full_path, const std::string& base_path) {
    std::error_code ec;
    std::filesystem::path relative = std::filesystem::relative(full_path, base_path, ec);
    if (ec) {
        return full_path;
    }
    return relative.string();
}

std::string get_line_context(const std::string& content, int line_number) {
    auto lines = split(content, '\n');
    if (line_number > 0 && line_number <= static_cast<int>(lines.size())) {
        return trim(lines[static_cast<size_t>(line_number - 1)]);
    }
    return "";
}

int find_line_number(const std::string& content, size_t position) {
    int line_number = 1;
    size_t limit = std::min(position, content.size());
    for (size_t i = 0; i < limit; ++i) {
        if (content[i] == '\n') {
            line_number++;
        }
    }
    return line_number;
}

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

namespace colors {

std::string RED;
std::string GREEN;
std::string YELLOW;
std::string BLUE;
std::string MAGENTA;
std::string CYAN;
std::string RESET;
std::string BOLD;

namespace {

bool stdout_is_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

bool enable_virtual_terminal() {
#ifdef _WIN32
    auto enable = [](DWORD handle_id) {
        HANDLE handle = GetStdHandle(handle_id);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
            return false;
        }
        DWORD mode = 0;
        if (!GetConsoleMode(handle, &mode)) {
            return false;
        }
        return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
    };
    bool output = enable(STD_OUTPUT_HANDLE);
    enable(STD_ERROR_HANDLE);
    return output;
#else
    return true;
#endif
}

void set_palette(bool enabled) {
    RED = enabled ? "\x1b[31m" : "";
    GREEN = enabled ? "\x1b[32m" : "";
    YELLOW = enabled ? "\x1b[33m" : "";
    BLUE = enabled ? "\x1b[34m" : "";
    MAGENTA = enabled ? "\x1b[35m" : "";
    CYAN = enabled ? "\x1b[36m" : "";
    RESET = enabled ? "\x1b[0m" : "";
    BOLD = enabled ? "\x1b[1m" : "";
}

}

void init() {
    bool enabled = stdout_is_tty();
    if (enabled) {
        enabled = enable_virtual_terminal();
    }
    set_palette(enabled);
}

void log(const std::string& message, const std::string& color) {
    std::cout << color << message << RESET << std::endl;
}

void error(const std::string& message) {
    std::cerr << RED << message << RESET << std::endl;
}

}

}
}
