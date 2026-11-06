#pragma once

#include "common.h"

#include <unordered_map>
#include <unordered_set>

namespace css_checker {

struct UsageIndex {
    std::unordered_map<std::string, std::vector<Usage>> usages;
    std::unordered_set<std::string> dynamic_prefixes;
    std::string opaque_file;
    int opaque_line = 0;
};

enum class ClassDisposition {
    Used,
    Unused,
    Unknown
};

struct ClassMatch {
    ClassDisposition disposition = ClassDisposition::Unused;
    std::vector<Usage> usages;
};

class UsageDetector {
public:
    explicit UsageDetector(const Config& config);

    UsageIndex build_index(const std::vector<std::string>& code_files,
                           const std::vector<std::string>& css_files = {}) const;
    ClassMatch classify(const UsageIndex& index, const std::string& class_name) const;

private:
    const Config& config_;
};

}
