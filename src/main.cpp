#include "common.h"
#include "cli_parser.h"
#include "file_scanner.h"
#include "css_parser.h"
#include "usage_detector.h"
#include "report_generator.h"

#include <filesystem>
#include <iostream>

using namespace css_checker;

namespace {

UnusedClass make_ref(const CssClass& css_class) {
    UnusedClass entry;
    entry.class_name = css_class.name;
    entry.css_file = css_class.file_path;
    entry.full_css_rule = css_class.context;
    return entry;
}

struct FileAnalysis {
    std::string path;
    std::vector<CssClass> classes;
    std::vector<UnusedClass> removable;
    std::vector<UnusedClass> kept;
    std::vector<UnusedClass> unknown;
    std::vector<std::string> removable_names;
};

std::vector<FileAnalysis> analyze_files(const std::vector<std::string>& css_files,
                                        CssParser& css_parser,
                                        UsageDetector& usage_detector,
                                        const UsageIndex& index) {
    std::vector<FileAnalysis> files;
    for (const auto& css_file : css_files) {
        auto content = utils::read_file(css_file);
        if (!content) {
            utils::colors::error("Warning: Could not open CSS file: " + css_file);
            continue;
        }

        FileAnalysis file;
        file.path = css_file;
        file.classes = css_parser.extract_classes(*content, css_file);
        auto uses = css_parser.selector_uses(*content);

        for (const auto& css_class : file.classes) {
            ClassMatch match = usage_detector.classify(index, css_class.name);
            if (match.disposition == ClassDisposition::Used) {
                continue;
            }
            if (match.disposition == ClassDisposition::Unknown) {
                file.unknown.push_back(make_ref(css_class));
                continue;
            }
            auto use = uses.find(css_class.name);
            bool exact = use != uses.end() && use->second.exact;
            if (exact) {
                file.removable.push_back(make_ref(css_class));
                file.removable_names.push_back(css_class.name);
            } else {
                file.kept.push_back(make_ref(css_class));
            }
        }
        files.push_back(std::move(file));
    }
    return files;
}

AnalysisResult to_result(const std::vector<FileAnalysis>& files,
                         size_t css_count,
                         size_t code_count,
                         const UsageIndex& index) {
    AnalysisResult result;
    result.total_css_files = css_count;
    result.total_code_files = code_count;
    result.opaque_file = index.opaque_file;
    result.opaque_line = index.opaque_line;
    for (const auto& file : files) {
        result.all_classes.insert(result.all_classes.end(), file.classes.begin(), file.classes.end());
        result.unused_classes.insert(result.unused_classes.end(), file.removable.begin(), file.removable.end());
        result.kept_classes.insert(result.kept_classes.end(), file.kept.begin(), file.kept.end());
        result.unknown_classes.insert(result.unknown_classes.end(), file.unknown.begin(), file.unknown.end());
    }
    return result;
}

void log_verbose(const FileAnalysis& file, UsageDetector& usage_detector, const UsageIndex& index) {
    for (const auto& css_class : file.classes) {
        ClassMatch match = usage_detector.classify(index, css_class.name);
        if (match.disposition == ClassDisposition::Used) {
            utils::colors::log("  + " + css_class.name + " (used " + std::to_string(match.usages.size()) + " time" +
                                   (match.usages.size() == 1 ? "" : "s") + ")",
                               utils::colors::GREEN);
        } else if (match.disposition == ClassDisposition::Unknown) {
            utils::colors::log("  ? " + css_class.name + " (dynamic, not removed)", utils::colors::YELLOW);
        }
    }
}

}

int main(int argc, char* argv[]) {
    try {
        utils::colors::init();

        CliParser cli_parser;
        Config config = cli_parser.parse(argc, argv);

        std::filesystem::path src_path(config.src_directory);
        if (!std::filesystem::exists(src_path) || !std::filesystem::is_directory(src_path)) {
            utils::colors::error("Error: Source directory does not exist: " + config.src_directory);
            return 2;
        }

        if (config.dry_run) {
            utils::colors::log("CSS Checker - Dry run (no files will be modified)", utils::colors::BOLD);
        } else {
            utils::colors::log("CSS Checker - Finding unused CSS classes", utils::colors::BOLD);
        }

        FileScanner file_scanner(config);
        CssParser css_parser(config);
        UsageDetector usage_detector(config);
        ReportGenerator report_generator(config);

        std::vector<std::string> css_files = file_scanner.get_css_files();
        std::vector<std::string> code_files = file_scanner.get_code_files();

        if (config.verbose) {
            utils::colors::log("\nFound " + std::to_string(css_files.size()) + " CSS files and " +
                                   std::to_string(code_files.size()) + " code files",
                               utils::colors::BLUE);
        }

        if (css_files.empty()) {
            utils::colors::error("No CSS files found in directory: " + config.src_directory);
            return 2;
        }

        bool refused_cleanup = false;
        if (code_files.empty()) {
            utils::colors::error("No code files found in " + config.src_directory +
                                 ". Every class would look unused.");
            if (config.cleanup_mode) {
                utils::colors::error("Cleanup skipped.");
                refused_cleanup = true;
                config.cleanup_mode = false;
            }
        }

        UsageIndex index = usage_detector.build_index(code_files, css_files);

        std::vector<FileAnalysis> findings =
            analyze_files(css_files, css_parser, usage_detector, index);

        bool cleanup_ran = false;
        bool cleanup_ok = true;
        if (config.cleanup_mode) {
            for (const auto& file : findings) {
                if (file.removable_names.empty()) {
                    continue;
                }
                cleanup_ran = true;
                if (!css_parser.cleanup_file(file.path, file.removable_names)) {
                    cleanup_ok = false;
                }
            }
            if (!config.dry_run && cleanup_ran) {
                findings = analyze_files(css_files, css_parser, usage_detector, index);
            }
        }

        for (const auto& file : findings) {
            report_generator.print_file_analysis(file.path, file.classes, file.removable, file.kept, file.unknown);
            if (config.verbose) {
                log_verbose(file, usage_detector, index);
            }
        }

        AnalysisResult result = to_result(findings, css_files.size(), code_files.size(), index);
        report_generator.print_analysis_summary(result, cleanup_ran, cleanup_ok, refused_cleanup);

        if (config.generate_report && (!result.unused_classes.empty() || !result.kept_classes.empty() ||
                                       !result.unknown_classes.empty())) {
            report_generator.generate_markdown_report(result);
        }

        if (refused_cleanup || (cleanup_ran && !cleanup_ok)) {
            return 2;
        }
        return result.unused_classes.empty() ? 0 : 1;

    } catch (const CliExit& status) {
        return status.code;
    } catch (const std::exception& e) {
        utils::colors::error("Error: " + std::string(e.what()));
        return 2;
    }
}
