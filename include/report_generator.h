#pragma once

#include "common.h"

namespace css_checker {

class ReportGenerator {
public:
    explicit ReportGenerator(const Config& config);

    void print_analysis_summary(const AnalysisResult& result,
                                bool cleanup_ran,
                                bool cleanup_ok,
                                bool cleanup_refused = false) const;
    void print_file_analysis(const std::string& css_file,
                             const std::vector<CssClass>& classes,
                             const std::vector<UnusedClass>& unused,
                             const std::vector<UnusedClass>& kept,
                             const std::vector<UnusedClass>& unknown) const;

    void generate_markdown_report(const AnalysisResult& result) const;

private:
    const Config& config_;

    std::string format_timestamp() const;
};

}
