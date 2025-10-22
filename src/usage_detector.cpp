#include "usage_detector.h"
#include <fstream>
#include <iostream>

namespace css_checker {

UsageDetector::UsageDetector(const Config& config) : config_(config) {}

std::vector<Usage> UsageDetector::find_class_usage(const std::string& class_name, 
                                                    const std::vector<std::string>& code_files) const {
    std::vector<Usage> all_usage;
    
    for (const auto& file_path : code_files) {
        try {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                if (config_.verbose) {
                    utils::colors::log("Warning: Could not open file: " + file_path, utils::colors::YELLOW);
                }
                continue;
            }
            
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();
            
            if (is_dynamically_used(class_name, content)) {
                Usage usage;
                usage.file_path = file_path;
                usage.line_number = 1;
                usage.context = "Dynamic usage detected";
                usage.usage_type = "dynamic";
                all_usage.push_back(usage);
                continue;
            }
            
            std::string extension = get_file_extension(file_path);
            std::vector<Usage> file_usage;
            
            if (extension == ".ts" || extension == ".tsx" || extension == ".js" || extension == ".jsx") {
                file_usage = detect_in_js_ts_file(class_name, file_path, content);
            }
            else if (extension == ".cshtml") {
                file_usage = detect_in_cshtml_file(class_name, file_path, content);
            }
            else if (extension == ".aspx") {
                file_usage = detect_in_aspx_file(class_name, file_path, content);
            }
            
            all_usage.insert(all_usage.end(), file_usage.begin(), file_usage.end());
            
        } catch (const std::exception& e) {
            if (config_.verbose) {
                utils::colors::log("Warning: Error processing file " + file_path + ": " + e.what(), 
                                   utils::colors::YELLOW);
            }
        }
    }
    
    return all_usage;
}

std::vector<Usage> UsageDetector::detect_in_js_ts_file(const std::string& class_name, 
                                                        const std::string& file_path, 
                                                        const std::string& content) const {
    std::vector<std::regex> patterns;
    
    std::string escaped_class = escape_regex(class_name);
    patterns.push_back(std::regex(R"(className=["']([^"']*\s+)?" + escaped_class + R"((\s+[^"']*)?["'])"));
    patterns.push_back(std::regex(R"(className=\{["']([^"']*\s+)?" + escaped_class + R"((\s+[^"']*)?["']\})"));
    
    patterns.push_back(std::regex(R"(clsx\([^)]*)" + escaped_class + R"([^)]*\))"));
    patterns.push_back(std::regex(R"(className=\{[^}]*\b)" + escaped_class + R"(\b[^}]*\})"));
    
    patterns.push_back(std::regex(R"(`[^`]*)" + escaped_class + R"([^`]*`)"));
    patterns.push_back(std::regex(R"("[^"]*)" + escaped_class + R"([^"]*")"));
    patterns.push_back(std::regex(R"('[^']*)") + escaped_class + R"([^']*')"));
    
    patterns.push_back(std::regex(R"(\b)" + escaped_class + R"(\b)"));
    
    return find_pattern_matches(class_name, file_path, content, patterns, "javascript");
}

std::vector<Usage> UsageDetector::detect_in_cshtml_file(const std::string& class_name, 
                                                         const std::string& file_path, 
                                                         const std::string& content) const {
    std::vector<std::regex> patterns;
    std::string escaped_class = escape_regex(class_name);
    
    // Standard HTML class attributes
    patterns.push_back(std::regex(R"(class=["']([^"']*\s+)?" + escaped_class + R"((\s+[^"']*)?["'])"));
    
    patterns.push_back(std::regex(R"(@class=["']([^"']*\s+)?" + escaped_class + R"((\s+[^"']*)?["'])"));
    
    patterns.push_back(std::regex(R"(@class=["']([^"']*\s+)?)"));
    patterns.push_back(std::regex(R"(Class=["']([^"']*\s+)?" + escaped_class + R"((\s+[^"']*)?["'])"));
    
    patterns.push_back(std::regex(R"(@\{[^}]*)" + escaped_class + R"([^}]*\})"));
    patterns.push_back(std::regex(R"(@\([^)]*)" + escaped_class + R"([^)]*\))"));
    
    patterns.push_back(std::regex(R"(\$"[^"]*)") + escaped_class + R"([^"]*")"));
    patterns.push_back(std::regex(R"(\$'[^']*)") + escaped_class + R"([^']*')"));
    
    patterns.push_back(std::regex(R"(Html\.[^(]*\([^)]*)" + escaped_class + R"([^)]*\))"));
    
    patterns.push_back(std::regex(R"(ViewBag[^"']*["'][^"']*)") + escaped_class + R"([^"']*["'])"));
    patterns.push_back(std::regex(R"(ViewData[^"']*["'][^"']*)") + escaped_class + R"([^"']*["'])"));
    
    patterns.push_back(std::regex(R"(<script[^>]*>[^<]*)" + escaped_class + R"([^<]*</script>)"));
    
    // CSS within style tags
    patterns.push_back(std::regex(R"(<style[^>]*>[^<]*\.)" + escaped_class + R"([^<]*</style>)"));
    
    return find_pattern_matches(class_name, file_path, content, patterns, "cshtml");
}

std::vector<Usage> UsageDetector::detect_in_aspx_file(const std::string& class_name, 
                                                       const std::string& file_path, 
                                                       const std::string& content) const {
    std::vector<std::regex> patterns;
    std::string escaped_class = escape_regex(class_name);
    
    // Standard HTML class attributes
    patterns.push_back(std::regex(R"(class=["']([^"']*\s+)?" + escaped_class + R"((\s+[^"']*)?["'])"));
    
    patterns.push_back(std::regex(R"(CssClass=["']([^"']*\s+)?)" + escaped_class + R"((\s+[^"']*)?["'])"));
    
    patterns.push_back(std::regex(R"(<asp:[^>]*CssClass=["'][^"']*)") + escaped_class + R"([^"']*["'][^>]*>)"));
    patterns.push_back(std::regex(R"(<asp:[^>]*class=["'][^"']*)") + escaped_class + R"([^"']*["'][^>]*>)"));
    
    patterns.push_back(std::regex(R"(<%[^%]*)") + escaped_class + R"([^%]*%>)"));
    patterns.push_back(std::regex(R"(<%=[^%]*)" + escaped_class + R"([^%]*%>)"));
    patterns.push_back(std::regex(R"(<%#[^%]*)") + escaped_class + R"([^%]*%>)"));
    
    patterns.push_back(std::regex(R"(Eval\([^)]*)" + escaped_class + R"([^)]*\))"));
    patterns.push_back(std::regex(R"(Bind\([^)]*)" + escaped_class + R"([^)]*\))"));
    
    patterns.push_back(std::regex(R"(<script[^>]*>[^<]*)" + escaped_class + R"([^<]*</script>)"));
    
    // CSS within style tags
    patterns.push_back(std::regex(R"(<style[^>]*>[^<]*\.)" + escaped_class + R"([^<]*</style>)"));
    
    patterns.push_back(std::regex(R"(<uc[0-9]*:[^>]*[Cc]lass[^=]*=["'][^"']*)")) + escaped_class + R"([^"']*["'][^>]*>)"));
    
    patterns.push_back(std::regex(R"(<asp:Content[^>]*>[^<]*)") + escaped_class + R"([^<]*</asp:Content>)"));
    
    return find_pattern_matches(class_name, file_path, content, patterns, "aspx");
}

bool UsageDetector::is_dynamically_used(const std::string& class_name, const std::string& content) const {
    std::vector<std::string> dynamic_prefixes = {
        "icon--", "button--", "tag--", "modal--", "toast--", 
        "tooltip--", "dropdown--", "loading--", "spinner--"
    };
    
    for (const auto& prefix : dynamic_prefixes) {
        if (class_name.find(prefix) == 0 && content.find(prefix + "${") != std::string::npos) {
            return true;
        }
    }
    
    if (content.find("`" + class_name + "`") != std::string::npos ||
        content.find("\"" + class_name + "\"") != std::string::npos ||
        content.find("'" + class_name + "'") != std::string::npos) {
        return true;
    }
    
    if (content.find("clsx(") != std::string::npos && content.find(class_name) != std::string::npos) {
        return true;
    }
    
    if (content.find("$\"") != std::string::npos && content.find(class_name) != std::string::npos) {
        return true;
    }
    
    return false;
}

std::string UsageDetector::get_file_extension(const std::string& file_path) const {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return file_path.substr(dot_pos);
    }
    return "";
}

std::string UsageDetector::escape_regex(const std::string& str) const {
    return utils::escape_regex(str);
}

int UsageDetector::find_line_number(const std::string& content, size_t position) const {
    return utils::find_line_number(content, position);
}

std::string UsageDetector::get_line_context(const std::string& content, int line_number) const {
    return utils::get_line_context(content, line_number);
}

std::vector<Usage> UsageDetector::find_pattern_matches(const std::string& class_name,
                                                       const std::string& file_path,
                                                       const std::string& content,
                                                       const std::vector<std::regex>& patterns,
                                                       const std::string& usage_type) const {
    std::vector<Usage> matches;
    
    for (const auto& pattern : patterns) {
        std::sregex_iterator iter(content.begin(), content.end(), pattern);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
            const std::smatch& match = *iter;
            
            Usage usage;
            usage.file_path = file_path;
            usage.line_number = find_line_number(content, match.position());
            usage.context = get_line_context(content, usage.line_number);
            usage.usage_type = usage_type;
            
            matches.push_back(usage);
        }
    }
    
    return matches;
}

}