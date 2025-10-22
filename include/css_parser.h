#pragma once

#include "common.h"

namespace css_checker {

class CssParser {
public:
    explicit CssParser(const Config& config);
    std::vector<CssClass> extract_classes(const std::string& css_file_path) const;
    std::string remove_unused_classes(const std::string& css_content, 
                                      const std::vector<std::string>& unused_classes) const;
    std::string get_line_context(const std::string& content, int line_number) const;

private:
    const Config& config_;
    bool should_ignore_class(const std::string& class_name) const;
    std::vector<std::string> get_lines(const std::string& content) const;
    int find_line_number(const std::string& content, size_t position) const;
};

}