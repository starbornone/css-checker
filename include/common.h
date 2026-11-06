#pragma once

#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace css_checker {

struct CssClass {
    std::string name;
    std::string file_path;
    int line_number = 0;
    std::string context;
};

struct Usage {
    std::string file_path;
    int line_number = 0;
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
    std::vector<UnusedClass> kept_classes;
    std::vector<UnusedClass> unknown_classes;
    std::string opaque_file;
    int opaque_line = 0;
    size_t total_css_files = 0;
    size_t total_code_files = 0;
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
};

namespace utils {
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string to_lower(const std::string& str);
    bool ends_with(const std::string& str, const std::string& suffix);
    bool ends_with_ignore_case(const std::string& str, const std::string& suffix);
    bool is_class_token(const std::string& token);
    std::string get_relative_path(const std::string& full_path, const std::string& base_path);
    std::string get_line_context(const std::string& content, int line_number);
    int find_line_number(const std::string& content, size_t position);
    std::optional<std::string> read_file(const std::string& path);

    namespace colors {
        extern std::string RED;
        extern std::string GREEN;
        extern std::string YELLOW;
        extern std::string BLUE;
        extern std::string MAGENTA;
        extern std::string CYAN;
        extern std::string RESET;
        extern std::string BOLD;

        void init();
        void log(const std::string& message, const std::string& color = RESET);
        void error(const std::string& message);
    }
}

}
