#include "common.h"
#include <algorithm>
#include <sstream>
#include <iostream>

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
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string get_relative_path(const std::string& full_path, const std::string& base_path) {
    if (full_path.find(base_path) == 0) {
        size_t start = base_path.length();
        if (start < full_path.length() && (full_path[start] == '/' || full_path[start] == '\\')) {
            start++;
        }
        return full_path.substr(start);
    }
    return full_path;
}

std::string escape_regex(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c == '.' || c == '^' || c == '$' || c == '*' || c == '+' || 
            c == '?' || c == '(' || c == ')' || c == '[' || c == ']' || 
            c == '{' || c == '}' || c == '\\' || c == '|') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

std::string get_line_context(const std::string& content, int line_number) {
    auto lines = split(content, '\n');
    if (line_number > 0 && line_number <= static_cast<int>(lines.size())) {
        return trim(lines[line_number - 1]);
    }
    return "";
}

int find_line_number(const std::string& content, size_t position) {
    int line_number = 1;
    for (size_t i = 0; i < position && i < content.length(); ++i) {
        if (content[i] == '\n') {
            line_number++;
        }
    }
    return line_number;
}

namespace colors {
#ifdef _WIN32
    const std::string RED = "";
    const std::string GREEN = "";
    const std::string YELLOW = "";
    const std::string BLUE = "";
    const std::string MAGENTA = "";
    const std::string CYAN = "";
    const std::string RESET = "";
    const std::string BOLD = "";
#else
    const std::string RED = "\x1b[31m";
    const std::string GREEN = "\x1b[32m";
    const std::string YELLOW = "\x1b[33m";
    const std::string BLUE = "\x1b[34m";
    const std::string MAGENTA = "\x1b[35m";
    const std::string CYAN = "\x1b[36m";
    const std::string RESET = "\x1b[0m";
    const std::string BOLD = "\x1b[1m";
#endif

void log(const std::string& message, const std::string& color) {
    std::cout << color << message << RESET << std::endl;
}

}
}
}