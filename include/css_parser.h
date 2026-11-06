#pragma once

#include "common.h"

#include <unordered_map>

namespace css_checker {

struct SelectorUse {
    bool exact = false;
    bool complex = false;
};

class CssParser {
public:
    explicit CssParser(const Config& config);

    std::vector<CssClass> extract_classes(const std::string& css_file_path) const;
    std::vector<CssClass> extract_classes(const std::string& css_content,
                                          const std::string& css_file_path) const;
    std::unordered_map<std::string, SelectorUse> selector_uses(const std::string& css_content) const;
    std::string remove_unused_classes(const std::string& css_content,
                                      const std::vector<std::string>& unused_classes) const;
    bool cleanup_file(const std::string& css_file,
                      const std::vector<std::string>& unused_classes) const;

private:
    const Config& config_;
    bool should_ignore_class(const std::string& class_name) const;
};

}
