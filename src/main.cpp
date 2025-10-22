#include "common.h"
#include "cli_parser.h"
#include "file_scanner.h"
#include "css_parser.h"
#include "usage_detector.h"
#include "report_generator.h"
#include <iostream>
#include <map>
#include <set>

using namespace css_checker;

int main(int argc, char* argv[]) {
    try {
        CliParser cli_parser;
        Config config = cli_parser.parse(argc, argv);
        
        if (!config.dry_run) {
            utils::colors::log("🔍 CSS Checker - Finding unused CSS classes...", utils::colors::BOLD);
        } else {
            utils::colors::log("🔍 CSS Checker - Dry run mode (no changes will be made)...", utils::colors::BOLD);
        }
        
        FileScanner file_scanner(config);
        CssParser css_parser(config);
        UsageDetector usage_detector(config);
        ReportGenerator report_generator(config);
        
        std::vector<std::string> css_files = file_scanner.get_css_files();
        std::vector<std::string> code_files = file_scanner.get_code_files();
        
        if (config.verbose) {
            utils::colors::log("\nFound " + std::to_string(css_files.size()) + " CSS files and " + 
                               std::to_string(code_files.size()) + " code files", utils::colors::BLUE);
        }
        
        if (css_files.empty()) {
            utils::colors::log("No CSS files found in directory: " + config.src_directory, utils::colors::YELLOW);
            return 1;
        }
        
        AnalysisResult result;
        result.total_css_files = css_files.size();
        result.total_code_files = code_files.size();
        
        for (const auto& css_file : css_files) {
            std::vector<CssClass> classes = css_parser.extract_classes(css_file);
            result.all_classes.insert(result.all_classes.end(), classes.begin(), classes.end());
            
            std::vector<UnusedClass> file_unused_classes;
            std::vector<std::string> unused_class_names;
            
            for (const auto& css_class : classes) {
                std::vector<Usage> usage = usage_detector.find_class_usage(css_class.name, code_files);
                
                if (usage.empty()) {
                    UnusedClass unused;
                    unused.class_name = css_class.name;
                    unused.css_file = css_file;
                    unused.full_css_rule = css_class.context;
                    
                    file_unused_classes.push_back(unused);
                    unused_class_names.push_back(css_class.name);
                } else {
                    result.usage_map[css_class.name] = usage;
                    
                    if (config.verbose) {
                        utils::colors::log("  ✓ " + css_class.name + " (used " + 
                                           std::to_string(usage.size()) + " time" + 
                                           (usage.size() > 1 ? "s" : "") + ")", utils::colors::GREEN);
                    }
                }
            }
            
            result.unused_classes.insert(result.unused_classes.end(), 
                                          file_unused_classes.begin(), file_unused_classes.end());
            
            report_generator.print_file_analysis(css_file, classes, file_unused_classes);
            
            if (config.cleanup_mode && !file_unused_classes.empty()) {
                report_generator.cleanup_css_file(css_file, unused_class_names);
            }
        }
        
        report_generator.print_analysis_summary(result);
        
        if (config.generate_report && !result.unused_classes.empty()) {
            report_generator.generate_markdown_report(result);
        }
        
        return result.unused_classes.empty() ? 0 : 1;
        
    } catch (const std::exception& e) {
        utils::colors::log("Error: " + std::string(e.what()), utils::colors::RED);
        return 1;
    }
}