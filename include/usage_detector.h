#pragma once

#include "common.h"

namespace css_checker {

class UsageDetector {
public:
    explicit UsageDetector(const Config& config);
    
    std::vector<Usage> find_class_usage(const std::string& class_name, 
                                         const std::vector<std::string>& code_files) const;
    
private:
    const Config& config_;
    
    std::vector<Usage> detect_in_js_ts_file(const std::string& class_name, 
                                             const std::string& file_path, 
                                             const std::string& content) const;
    
    std::vector<Usage> detect_in_cshtml_file(const std::string& class_name, 
                                              const std::string& file_path, 
                                              const std::string& content) const;
    
    std::vector<Usage> detect_in_aspx_file(const std::string& class_name, 
                                            const std::string& file_path, 
                                            const std::string& content) const;
    
    bool is_dynamically_used(const std::string& class_name, const std::string& content) const;
    std::string get_file_extension(const std::string& file_path) const;
    std::string escape_regex(const std::string& str) const;
    int find_line_number(const std::string& content, size_t position) const;
    std::string get_line_context(const std::string& content, int line_number) const;
    
    std::vector<Usage> find_pattern_matches(const std::string& class_name,
                                             const std::string& file_path,
                                             const std::string& content,
                                             const std::vector<std::regex>& patterns,
                                             const std::string& usage_type) const;
};

}