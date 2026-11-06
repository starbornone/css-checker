#include "report_generator.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace css_checker {
namespace {

std::string markdown_code(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        if (c == '`') {
            escaped += "'";
        } else {
            escaped.push_back(c);
        }
    }
    return escaped;
}

}

ReportGenerator::ReportGenerator(const Config& config) : config_(config) {}

void ReportGenerator::print_analysis_summary(const AnalysisResult& result,
                                             bool cleanup_ran,
                                             bool cleanup_ok,
                                             bool cleanup_refused) const {
    utils::colors::log("\nAnalysis summary", utils::colors::BOLD);
    utils::colors::log("=======================================", utils::colors::BOLD);

    utils::colors::log("CSS files found: " + std::to_string(result.total_css_files), utils::colors::BLUE);
    utils::colors::log("Code files found: " + std::to_string(result.total_code_files), utils::colors::BLUE);
    utils::colors::log("Total CSS classes: " + std::to_string(result.all_classes.size()), utils::colors::BLUE);

    std::string unused_color = result.unused_classes.empty() ? utils::colors::GREEN : utils::colors::RED;
    utils::colors::log("Unused classes: " + std::to_string(result.unused_classes.size()), unused_color);
    if (!result.kept_classes.empty()) {
        utils::colors::log("Kept in compound or nested selectors: " + std::to_string(result.kept_classes.size()),
                           utils::colors::YELLOW);
    }
    if (!result.unknown_classes.empty()) {
        utils::colors::log("Dynamic classes left in place: " + std::to_string(result.unknown_classes.size()),
                           utils::colors::YELLOW);
        if (!result.opaque_file.empty()) {
            std::string where = utils::get_relative_path(result.opaque_file, config_.src_directory);
            if (result.opaque_line > 0) {
                where += ":" + std::to_string(result.opaque_line);
            }
            utils::colors::log("Computed class binding at " + where +
                                   ". Names without a literal use were left in place.",
                               utils::colors::YELLOW);
        }
    }

    if (result.unused_classes.empty() && result.kept_classes.empty() && result.unknown_classes.empty()) {
        utils::colors::log("\nAll CSS classes are used.", utils::colors::GREEN);
        return;
    }

    if (cleanup_refused) {
        utils::colors::log("\nCleanup was skipped because no code files were found.",
                           utils::colors::YELLOW);
    } else if (config_.dry_run) {
        utils::colors::log("\nDry run: no files were modified. Apply changes with --cleanup and without --dry-run.",
                           utils::colors::YELLOW);
    } else if (cleanup_ran && cleanup_ok) {
        utils::colors::log("\nCleanup completed. Existing .backup files were left untouched.",
                           utils::colors::GREEN);
    } else if (cleanup_ran && !cleanup_ok) {
        utils::colors::log("\nCleanup did not finish. Stylesheets that failed were not replaced.",
                           utils::colors::RED);
    } else if (!result.unused_classes.empty()) {
        utils::colors::log("\nRun with --cleanup to delete rules whose selector is exactly an unused class.",
                           utils::colors::YELLOW);
    } else if (!result.kept_classes.empty()) {
        utils::colors::log("\nCompound and nested selectors were kept. No exact-selector rule remains to remove.",
                           utils::colors::YELLOW);
    }
}

void ReportGenerator::print_file_analysis(const std::string& css_file,
                                          const std::vector<CssClass>& classes,
                                          const std::vector<UnusedClass>& unused,
                                          const std::vector<UnusedClass>& kept,
                                          const std::vector<UnusedClass>& unknown) const {
    std::string relative_path = utils::get_relative_path(css_file, config_.src_directory);

    utils::colors::log("\nAnalyzing: " + relative_path, utils::colors::BOLD);
    utils::colors::log("Found " + std::to_string(classes.size()) + " CSS classes");

    if (!unused.empty()) {
        utils::colors::log("  Unused classes:", utils::colors::RED);
        for (const auto& unused_class : unused) {
            utils::colors::log("    - ." + unused_class.class_name, utils::colors::RED);
        }
    } else if (kept.empty() && unknown.empty()) {
        utils::colors::log("  All classes in this file are used.", utils::colors::GREEN);
    }

    if (!kept.empty()) {
        utils::colors::log("  Kept (selector is not exactly this class):", utils::colors::YELLOW);
        for (const auto& kept_class : kept) {
            utils::colors::log("    - ." + kept_class.class_name, utils::colors::YELLOW);
        }
    }

    if (!unknown.empty()) {
        utils::colors::log("  Dynamic classes (not removed):", utils::colors::YELLOW);
        for (const auto& unknown_class : unknown) {
            utils::colors::log("    - ." + unknown_class.class_name, utils::colors::YELLOW);
        }
    }
}

void ReportGenerator::generate_markdown_report(const AnalysisResult& result) const {
    std::ofstream report_file(config_.report_path);
    if (!report_file) {
        utils::colors::error("Error: Could not create report file: " + config_.report_path);
        return;
    }

    report_file << "# Unused CSS Classes Report\n\n";
    report_file << "Generated on: " << format_timestamp() << "\n\n";
    report_file << "Total unused classes: **" << result.unused_classes.size() << "**\n\n";
    report_file << "Kept in compound or nested selectors: **" << result.kept_classes.size() << "**\n\n";
    report_file << "Dynamic classes left in place: **" << result.unknown_classes.size() << "**\n\n";
    if (!result.opaque_file.empty()) {
        std::string where = utils::get_relative_path(result.opaque_file, config_.src_directory);
        if (result.opaque_line > 0) {
            where += ":" + std::to_string(result.opaque_line);
        }
        report_file << "Computed class binding at `" << markdown_code(where)
                    << "`. Names without a literal use were left in place.\n\n";
    }

    auto write_group = [&](const std::vector<UnusedClass>& classes, const std::string& heading) {
        if (classes.empty()) {
            return;
        }
        report_file << "## " << heading << "\n\n";
        std::map<std::string, std::vector<UnusedClass>> by_file;
        for (const auto& entry : classes) {
            by_file[entry.css_file].push_back(entry);
        }
        for (const auto& [file, entries] : by_file) {
            std::string relative_path = utils::get_relative_path(file, config_.src_directory);
            report_file << "### " << relative_path << "\n\n";
            for (const auto& entry : entries) {
                report_file << "- `." << markdown_code(entry.class_name) << "`";
                if (!entry.full_css_rule.empty()) {
                    report_file << " (`" << markdown_code(entry.full_css_rule) << "`)";
                }
                report_file << "\n";
            }
            report_file << "\n";
        }
    };

    write_group(result.unused_classes, "Unused classes");
    write_group(result.kept_classes, "Kept selectors");
    write_group(result.unknown_classes, "Dynamic classes");

    report_file << "## Cleanup\n\n";
    report_file << "`--cleanup` deletes a rule only when one of its selectors is exactly the unused class,\n";
    report_file << "with optional pseudos. Compound selectors such as `.used .unused` are left unchanged.\n";
    report_file << "The first `.backup` file is kept. Dynamic classes are never deleted.\n\n";

    report_file << "## Configuration\n\n";
    report_file << "- Source directory: `" << config_.src_directory << "`\n";
    report_file << "- CSS extensions: ";
    for (size_t i = 0; i < config_.css_extensions.size(); ++i) {
        if (i > 0) report_file << ", ";
        report_file << "`" << config_.css_extensions[i] << "`";
    }
    report_file << "\n- Code extensions: ";
    for (size_t i = 0; i < config_.code_extensions.size(); ++i) {
        if (i > 0) report_file << ", ";
        report_file << "`" << config_.code_extensions[i] << "`";
    }
    report_file << "\n";

    utils::colors::log("\nReport generated: " + config_.report_path, utils::colors::CYAN);
}

std::string ReportGenerator::format_timestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm time_parts{};
#if defined(_WIN32)
    gmtime_s(&time_parts, &time);
#else
    gmtime_r(&time, &time_parts);
#endif
    std::ostringstream out;
    out << std::put_time(&time_parts, "%Y-%m-%d %H:%M:%S UTC");
    return out.str();
}

}
