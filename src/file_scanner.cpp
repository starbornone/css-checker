#include "file_scanner.h"

#include <exception>

namespace css_checker {
namespace {

bool is_skipped_directory(const std::string& name) {
    std::string lower = utils::to_lower(name);
    return lower == "node_modules" || lower == "bin" || lower == "obj" || lower == ".git";
}

}

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
            utils::colors::error("Error: Source directory does not exist: " + config_.src_directory);
            return result;
        }

        if (!std::filesystem::is_directory(src_path)) {
            utils::colors::error("Error: Source path is not a directory: " + config_.src_directory);
            return result;
        }

        scan_directory_recursive(src_path, extensions, result);

    } catch (const std::filesystem::filesystem_error& e) {
        utils::colors::error("Filesystem error: " + std::string(e.what()));
    } catch (const std::exception& e) {
        utils::colors::error("Error scanning files: " + std::string(e.what()));
    }

    return result;
}

bool FileScanner::has_extension(const std::string& file_path,
                                const std::vector<std::string>& extensions) const {
    for (const auto& ext : extensions) {
        if (utils::ends_with_ignore_case(file_path, ext)) {
            return true;
        }
    }
    return false;
}

void FileScanner::scan_directory_recursive(const std::filesystem::path& dir,
                                           const std::vector<std::string>& extensions,
                                           std::vector<std::string>& result) const {
    try {
        std::filesystem::directory_options options =
            std::filesystem::directory_options::skip_permission_denied;
        for (const auto& entry : std::filesystem::directory_iterator(dir, options)) {
            std::error_code ec;
            if (entry.is_symlink(ec)) {
                if (!entry.is_regular_file(ec)) {
                    continue;
                }
            } else if (entry.is_directory(ec)) {
                if (is_skipped_directory(entry.path().filename().string())) {
                    continue;
                }
                scan_directory_recursive(entry.path(), extensions, result);
                continue;
            }

            if (!entry.is_regular_file(ec)) {
                continue;
            }

            std::string file_path = entry.path().string();
            if (!has_extension(file_path, extensions)) {
                continue;
            }

            result.push_back(file_path);
            if (config_.verbose) {
                utils::colors::log("Found: " + utils::get_relative_path(file_path, config_.src_directory),
                                   utils::colors::BLUE);
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        utils::colors::error("Warning: Could not scan directory " + dir.string() + ": " + e.what());
    }
}

}
