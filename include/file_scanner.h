#pragma once

#include "common.h"
#include <filesystem>

namespace css_checker {

class FileScanner {
public:
    explicit FileScanner(const Config& config);
    
    std::vector<std::string> get_css_files() const;
    std::vector<std::string> get_code_files() const;
    
    std::vector<std::string> scan_files(const std::vector<std::string>& extensions) const;
    
private:
    const Config& config_;
    
    bool has_extension(const std::string& file_path, const std::vector<std::string>& extensions) const;
    void scan_directory_recursive(const std::filesystem::path& dir, 
                                   const std::vector<std::string>& extensions,
                                   std::vector<std::string>& result) const;
};

}