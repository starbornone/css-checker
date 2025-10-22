#include "report_generator.h"
#include "css_parser.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <filesystem>

namespace css_checker {

ReportGenerator::ReportGenerator(const Config& config) : config_(config) {}

void ReportGenerator::print_analysis_summary(const AnalysisResult& result) const {
    utils::colors::log("\n📊 Analysis Summary", utils::colors::BOLD);
    utils::colors::log("═══════════════════════════════════════", utils::colors::BOLD);
    
    utils::colors::log("CSS files found: " + std::to_string(result.total_css_files), utils::colors::BLUE);
    utils::colors::log("Code files found: " + std::to_string(result.total_code_files), utils::colors::BLUE);
    utils::colors::log("Total CSS classes: " + std::to_string(result.all_classes.size()), utils::colors::BLUE);
    
    int total_unused = result.unused_classes.size();
    std::string unused_color = total_unused > 0 ? utils::colors::RED : utils::colors::GREEN;
    utils::colors::log("Unused classes: " + std::to_string(total_unused), unused_color);
    
    if (total_unused == 0) {
        utils::colors::log("\n🎉 All CSS classes are being used!", utils::colors::GREEN);
    } else {
        if (config_.dry_run) {
            utils::colors::log("\n💡 This is a dry run. Use without --dry-run to perform cleanup.", utils::colors::YELLOW);
        } else if (config_.cleanup_mode) {
            utils::colors::log("\n✅ Cleanup completed! Backup files created with .backup extension.", utils::colors::GREEN);
        } else {
            utils::colors::log("\n💡 Consider removing these unused classes to reduce bundle size.", utils::colors::YELLOW);
        }
    }
}

void ReportGenerator::print_file_analysis(const std::string& css_file, 
                                           const std::vector<CssClass>& classes,
                                           const std::vector<UnusedClass>& unused) const {
    std::string relative_path = utils::get_relative_path(css_file, config_.src_directory);
    
    utils::colors::log("\n" + utils::colors::BOLD + "Analyzing: " + relative_path + utils::colors::RESET);
    utils::colors::log("Found " + std::to_string(classes.size()) + " CSS classes");
    
    if (config_.verbose) {
        for (const auto& css_class : classes) {
            bool is_unused = std::any_of(unused.begin(), unused.end(), 
                [&css_class](const UnusedClass& u) { return u.class_name == css_class.name; });
            
            if (!is_unused) {
                auto usage_it = css_class.name;
                utils::colors::log("  ✓ " + css_class.name, utils::colors::GREEN);
            }
        }
    }
    
    if (!unused.empty()) {
        utils::colors::log("\n  " + utils::colors::RED + "Unused classes:" + utils::colors::RESET);
        for (const auto& unused_class : unused) {
            utils::colors::log("    - ." + unused_class.class_name, utils::colors::RED);
        }
    } else {
        utils::colors::log("  " + utils::colors::GREEN + "All classes are being used!" + utils::colors::RESET);
    }
}

void ReportGenerator::generate_markdown_report(const AnalysisResult& result) const {
    std::ofstream report_file(config_.report_path);
    if (!report_file.is_open()) {
        utils::colors::log("Error: Could not create report file: " + config_.report_path, utils::colors::RED);
        return;
    }
    
    report_file << "# Unused CSS Classes Report\n\n";
    report_file << "Generated on: " << format_timestamp() << "\n\n";
    report_file << "Total unused classes: **" << result.unused_classes.size() << "**\n\n";
    
    report_file << "## Summary by File\n\n";
    
    std::map<std::string, std::vector<UnusedClass>> by_file;
    for (const auto& unused : result.unused_classes) {
        by_file[unused.css_file].push_back(unused);
    }
    
    for (const auto& [file, unused_list] : by_file) {
        std::string relative_path = utils::get_relative_path(file, config_.src_directory);
        report_file << "### " << relative_path << "\n\n";
        report_file << "**" << unused_list.size() << " unused classes:**\n\n";
        
        for (const auto& unused : unused_list) {
            report_file << "- `." << unused.class_name << "`\n";
        }
        report_file << "\n";
    }
    
    report_file << "## Cleanup Suggestions\n\n";
    report_file << "1. Review each unused class to ensure it's truly not needed\n";
    report_file << "2. Consider if these classes might be used in future features\n";
    report_file << "3. Remove classes that are definitely unused to reduce bundle size\n";
    report_file << "4. Run `css_checker --dry-run` to see what would be removed\n\n";
    
    report_file << "## Commands\n\n";
    report_file << "- `css_checker` - Report unused classes\n";
    report_file << "- `css_checker --dry-run` - Show what would be cleaned up\n";
    report_file << "- `css_checker --cleanup` - Actually remove unused classes (creates backups)\n";
    report_file << "- `css_checker --verbose` - Show detailed information\n\n";
    
    report_file << "## Configuration Used\n\n";
    report_file << "- Source directory: `" << config_.src_directory << "`\n";
    report_file << "- CSS extensions: ";
    for (size_t i = 0; i < config_.css_extensions.size(); ++i) {
        if (i > 0) report_file << ", ";
        report_file << "`" << config_.css_extensions[i] << "`";
    }
    report_file << "\n";
    report_file << "- Code extensions: ";
    for (size_t i = 0; i < config_.code_extensions.size(); ++i) {
        if (i > 0) report_file << ", ";
        report_file << "`" << config_.code_extensions[i] << "`";
    }
    report_file << "\n\n";
    
    report_file.close();
    
    std::string relative_report_path = utils::get_relative_path(config_.report_path, ".");
    utils::colors::log("\n📄 Report generated: " + relative_report_path, utils::colors::CYAN);
}

bool ReportGenerator::create_backup(const std::string& file_path) const {
    try {
        std::string backup_path = get_backup_filename(file_path);
        std::filesystem::copy_file(file_path, backup_path, 
                                   std::filesystem::copy_options::overwrite_existing);
        
        std::string relative_backup = utils::get_relative_path(backup_path, config_.src_directory);
        utils::colors::log("    Backup created: " + relative_backup, utils::colors::YELLOW);
        return true;
    } catch (const std::exception& e) {
        utils::colors::log("Error creating backup for " + file_path + ": " + e.what(), utils::colors::RED);
        return false;
    }
}

bool ReportGenerator::cleanup_css_file(const std::string& css_file, 
                                        const std::vector<std::string>& unused_classes) const {
    if (config_.dry_run) {
        utils::colors::log("    [DRY RUN] Would remove " + std::to_string(unused_classes.size()) + 
                           " unused classes", utils::colors::YELLOW);
        return true;
    }
    
    try {
        if (!create_backup(css_file)) {
            return false;
        }
        
        std::ifstream file(css_file);
        if (!file.is_open()) {
            utils::colors::log("Error: Could not open CSS file for cleanup: " + css_file, utils::colors::RED);
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        
        CssParser parser(config_);
        std::string cleaned_content = parser.remove_unused_classes(content, unused_classes);
        
        std::ofstream out_file(css_file);
        if (!out_file.is_open()) {
            utils::colors::log("Error: Could not write cleaned CSS file: " + css_file, utils::colors::RED);
            return false;
        }
        
        out_file << cleaned_content;
        out_file.close();
        
        utils::colors::log("    Cleaned up " + std::to_string(unused_classes.size()) + 
                           " unused classes", utils::colors::GREEN);
        return true;
        
    } catch (const std::exception& e) {
        utils::colors::log("Error cleaning up " + css_file + ": " + e.what(), utils::colors::RED);
        return false;
    }
}

std::string ReportGenerator::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

std::string ReportGenerator::get_backup_filename(const std::string& original_file) const {
    return original_file + ".backup";
}

}