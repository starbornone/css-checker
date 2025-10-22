#include "css_parser.h"
#include <fstream>
#include <regex>
#include <iostream>

namespace css_checker {

CssParser::CssParser(const Config& config) : config_(config) {}

std::vector<CssClass> CssParser::extract_classes(const std::string& css_file_path) const {
    std::vector<CssClass> classes;
    
    std::ifstream file(css_file_path);
    if (!file.is_open()) {
        utils::colors::log("Warning: Could not open CSS file: " + css_file_path, utils::colors::YELLOW);
        return classes;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    std::regex class_regex(R"(\.([a-zA-Z][a-zA-Z0-9_-]*))");;
    std::sregex_iterator iter(content.begin(), content.end(), class_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        const std::smatch& match = *iter;
        std::string class_name = match[1].str();
        
        if (should_ignore_class(class_name)) {
            continue;
        }
        
        CssClass css_class;
        css_class.name = class_name;
        css_class.file_path = css_file_path;
        css_class.line_number = utils::find_line_number(content, match.position());
        
        css_class.context = utils::get_line_context(content, css_class.line_number);
        
        classes.push_back(css_class);
    }
    
    return classes;
}

std::string CssParser::remove_unused_classes(const std::string& css_content, 
                                              const std::vector<std::string>& unused_classes) const {
    std::string cleaned_content = css_content;
    
    for (const std::string& class_name : unused_classes) {
        std::string escaped_class = utils::escape_regex(class_name);
        std::string pattern = R"(\." + escaped_class + R"([^{]*\{[^}]*\})";
        std::regex rule_regex(pattern);
        
        cleaned_content = std::regex_replace(cleaned_content, rule_regex, "");
    }
    
    std::regex empty_lines_regex(R"(\n\s*\n\s*\n)");
    cleaned_content = std::regex_replace(cleaned_content, empty_lines_regex, "\n\n");
    
    return cleaned_content;
}

bool CssParser::should_ignore_class(const std::string& class_name) const {
    for (const auto& pattern : config_.ignore_patterns) {
        if (std::regex_match(class_name, pattern)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> CssParser::get_lines(const std::string& content) const {
    return utils::split(content, '\n');
}

int CssParser::find_line_number(const std::string& content, size_t position) const {
    return utils::find_line_number(content, position);
}

std::string CssParser::get_line_context(const std::string& content, int line_number) const {
    return utils::get_line_context(content, line_number);
}

}