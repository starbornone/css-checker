#include "cli_parser.h"
#include <iostream>
#include <algorithm>
#include <sstream>

namespace css_checker {

CliParser::CliParser() {}

Config CliParser::parse(int argc, char* argv[]) const {
    Config config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_help();
            exit(0);
        }
        else if (arg == "--version" || arg == "-v") {
            print_version();
            exit(0);
        }
        else if (arg == "--verbose") {
            config.verbose = true;
        }
        else if (arg == "--dry-run") {
            config.dry_run = true;
        }
        else if (arg == "--cleanup") {
            config.cleanup_mode = true;
        }
        else if (arg == "--no-report") {
            config.generate_report = false;
        }
        else if (arg == "--src" || arg == "-s") {
            if (i + 1 < argc) {
                config.src_directory = argv[++i];
            } else {
                std::cerr << "Error: --src requires a directory path\n";
                exit(1);
            }
        }
        else if (arg == "--report" || arg == "-r") {
            if (i + 1 < argc) {
                config.report_path = argv[++i];
            } else {
                std::cerr << "Error: --report requires a file path\n";
                exit(1);
            }
        }
        else if (arg == "--css-ext") {
            if (i + 1 < argc) {
                config.css_extensions.clear();
                parse_extensions(argv[++i], config.css_extensions);
            } else {
                std::cerr << "Error: --css-ext requires a comma-separated list of extensions\n";
                exit(1);
            }
        }
        else if (arg == "--code-ext") {
            if (i + 1 < argc) {
                config.code_extensions.clear();
                parse_extensions(argv[++i], config.code_extensions);
            } else {
                std::cerr << "Error: --code-ext requires a comma-separated list of extensions\n";
                exit(1);
            }
        }
        else if (arg.front() == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            exit(1);
        }
        else {
            if (config.src_directory == "src") {
                config.src_directory = arg;
            } else {
                std::cerr << "Error: Unexpected argument: " << arg << "\n";
                exit(1);
            }
        }
    }
    
    return config;
}

void CliParser::print_help() const {
    std::cout << "CSS Checker - Find and remove unused CSS classes\n\n";
    std::cout << "USAGE:\n";
    std::cout << "    css_checker [OPTIONS] [SOURCE_DIRECTORY]\n\n";
    std::cout << "OPTIONS:\n";
    std::cout << "    -h, --help          Show this help message\n";
    std::cout << "    -v, --version       Show version information\n";
    std::cout << "    -s, --src <DIR>     Source directory to scan (default: src)\n";
    std::cout << "    -r, --report <FILE> Report file path (default: unused-css-report.md)\n";
    std::cout << "    --verbose           Show detailed output\n";
    std::cout << "    --dry-run           Show what would be done without making changes\n";
    std::cout << "    --cleanup           Remove unused CSS classes (creates backups)\n";
    std::cout << "    --no-report         Don't generate markdown report\n";
    std::cout << "    --css-ext <EXTS>    CSS file extensions (default: .css)\n";
    std::cout << "    --code-ext <EXTS>   Code file extensions (default: .tsx,.ts,.js,.jsx,.cshtml,.aspx)\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "    css_checker                           # Analyze 'src' directory\n";
    std::cout << "    css_checker /path/to/project          # Analyze specific directory\n";
    std::cout << "    css_checker --verbose                 # Show detailed output\n";
    std::cout << "    css_checker --dry-run                 # Preview what would be cleaned\n";
    std::cout << "    css_checker --cleanup                 # Actually remove unused classes\n";
    std::cout << "    css_checker --src webapp --verbose    # Analyze 'webapp' directory with details\n";
    std::cout << "    css_checker --code-ext .ts,.tsx,.vue  # Custom code file extensions\n\n";
    std::cout << "SUPPORTED FILE TYPES:\n";
    std::cout << "    CSS:        .css files\n";
    std::cout << "    JavaScript: .js, .jsx, .ts, .tsx files\n";
    std::cout << "    Razor:      .cshtml files (ASP.NET Core)\n";
    std::cout << "    Web Forms:  .aspx files (ASP.NET Framework)\n\n";
    std::cout << "The tool creates backup files (.backup extension) when using --cleanup.\n";
}

void CliParser::print_version() const {
    std::cout << "CSS Checker v1.0.0\n";
    std::cout << "A fast C++ tool for finding unused CSS classes in web projects.\n";
    std::cout << "Supports JavaScript/TypeScript, Razor (.cshtml), and Web Forms (.aspx) files.\n";
}

void CliParser::parse_extensions(const std::string& ext_string, std::vector<std::string>& extensions) const {
    std::stringstream ss(ext_string);
    std::string item;
    
    while (std::getline(ss, item, ',')) {
        item = utils::trim(item);
        if (!item.empty()) {
            if (item.front() != '.') {
                item = "." + item;
            }
            extensions.push_back(item);
        }
    }
}

}