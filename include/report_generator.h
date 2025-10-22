#pragma once

#include "common.h"

namespace css_checker {

class ReportGenerator {
public:
    explicit ReportGenerator(const Config& config);
    
    void print_analysis_summary(const AnalysisResult& result) const;
    void print_file_analysis(const std::string& css_file, 
                             const std::vector<CssClass>& classes,
                             const std::vector<UnusedClass>& unused) const;
    
    void generate_markdown_report(const AnalysisResult& result) const;
    
    bool create_backup(const std::string& file_path) const;
    bool cleanup_css_file(const std::string& css_file, 
                          const std::vector<std::string>& unused_classes) const;
    
private:
    const Config& config_;
    
    std::string format_timestamp() const;
    std::string get_backup_filename(const std::string& original_file) const;
};

}