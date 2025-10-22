#pragma once

#include "common.h"

namespace css_checker {

class CliParser {
public:
    CliParser();

    Config parse(int argc, char* argv[]) const;

    void print_help() const;
    void print_version() const;
    
private:
    void parse_extensions(const std::string& ext_string, std::vector<std::string>& extensions) const;
};

}