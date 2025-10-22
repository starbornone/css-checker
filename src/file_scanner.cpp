#include "file_scanner.h"
#include <iostream>

namespace css_checker {

FileScanner::FileScanner(const Config& config) : config_(config) {}

std::vector<std::string> FileScanner::get_css_files() const {
    return scan_files(config_.css_extensions);
}

std::vector<std::string> FileScanner::get_code_files() const {
    return scan_files(config_.code_extensions);
}

std::vector<std::string> FileScanner::scan_files(const std::vector<std::string>& extensions) const {
    std::vector<std::string> result;
    
    try {
        std::filesystem::path src_path(config_.src_directory);
        
        if (!std::filesystem::exists(src_path)) {
            utils::colors::log("Error: Source directory does not exist: " + config_.src_directory, 
                               utils::colors::RED);
            return result;
        }
        
        if (!std::filesystem::is_directory(src_path)) {
            utils::colors::log("Error: Source path is not a directory: " + config_.src_directory, 
                               utils::colors::RED);
            return result;
        }
        
        scan_directory_recursive(src_path, extensions, result);
        
    } catch (const std::filesystem::filesystem_error& e) {
        utils::colors::log("Filesystem error: " + std::string(e.what()), utils::colors::RED);
    } catch (const std::exception& e) {
        utils::colors::log("Error scanning files: " + std::string(e.what()), utils::colors::RED);
    }
    
    return result;
}

bool FileScanner::has_extension(const std::string& file_path, 
                                 const std::vector<std::string>& extensions) const {
    for (const auto& ext : extensions) {
        if (utils::ends_with(file_path, ext)) {
            return true;
        }
    }
    return false;
}

void FileScanner::scan_directory_recursive(const std::filesystem::path& dir, 
                                            const std::vector<std::string>& extensions,
                                            std::vector<std::string>& result) const {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                std::string file_path = entry.path().string();
                
                if (has_extension(file_path, extensions)) {
                    result.push_back(file_path);
                    
                    if (config_.verbose) {
                        utils::colors::log("Found: " + utils::get_relative_path(file_path, config_.src_directory), 
                                           utils::colors::BLUE);
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        utils::colors::log("Warning: Could not scan directory " + dir.string() + ": " + e.what(), 
                           utils::colors::YELLOW);
    }
}

}