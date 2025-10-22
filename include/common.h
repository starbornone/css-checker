#pragma once

#include <string>
#include <vector>
#include <set>
#include <regex>
#include <map>

namespace css_checker {

class CssParser;
class FileScanner;
class UsageDetector;
class ReportGenerator;
struct CssClass {
    std::string name;
    std::string file_path;
    int line_number;
    std::string context;
};

struct Usage {
    std::string file_path;
    int line_number;
    std::string context;
    std::string usage_type;
};

struct UnusedClass {
    std::string class_name;
    std::string css_file;
    std::string full_css_rule;
};

struct AnalysisResult {
    std::vector<CssClass> all_classes;
    std::vector<UnusedClass> unused_classes;
    std::map<std::string, std::vector<Usage>> usage_map;
    int total_css_files;
    int total_code_files;
};

struct Config {
    std::string src_directory = "src";
    std::vector<std::string> css_extensions = {".css"};
    std::vector<std::string> code_extensions = {".tsx", ".ts", ".js", ".jsx", ".cshtml", ".aspx"};
    std::vector<std::regex> ignore_patterns;
    bool verbose = false;
    bool dry_run = false;
    bool generate_report = true;
    bool cleanup_mode = false;
    std::string report_path = "unused-css-report.md";
    
    Config() {
        ignore_patterns.push_back(std::regex("^\\."));
        ignore_patterns.push_back(std::regex("^@"));
        ignore_patterns.push_back(std::regex("^[a-z]+$"));
    }
};

namespace utils {
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string to_lower(const std::string& str);
    bool ends_with(const std::string& str, const std::string& suffix);
    std::string get_relative_path(const std::string& full_path, const std::string& base_path);
    std::string escape_regex(const std::string& str);
    std::string get_line_context(const std::string& content, int line_number);
    int find_line_number(const std::string& content, size_t position);
    
    namespace colors {
        extern const std::string RED;
        extern const std::string GREEN;
        extern const std::string YELLOW;
        extern const std::string BLUE;
        extern const std::string MAGENTA;
        extern const std::string CYAN;
        extern const std::string RESET;
        extern const std::string BOLD;
        
        void log(const std::string& message, const std::string& color = RESET);
    }
}

}