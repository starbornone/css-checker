#include "cli_parser.h"

#include <iostream>
#include <sstream>

namespace css_checker {
namespace {

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "Error: " << message << "\n";
    throw CliExit{2};
}

}

CliParser::CliParser() {}

Config CliParser::parse(int argc, char* argv[]) const {
    Config config;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i] == nullptr ? std::string() : std::string(argv[i]);

        if (arg == "--help" || arg == "-h") {
            print_help();
            throw CliExit{0};
        }
        if (arg == "--version" || arg == "-v") {
            print_version();
            throw CliExit{0};
        }
        if (arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--dry-run") {
            config.dry_run = true;
        } else if (arg == "--cleanup") {
            config.cleanup_mode = true;
        } else if (arg == "--no-report") {
            config.generate_report = false;
        } else if (arg == "--src" || arg == "-s") {
            if (i + 1 >= argc) {
                fail("--src requires a directory path");
            }
            config.src_directory = argv[++i];
        } else if (arg == "--report" || arg == "-r") {
            if (i + 1 >= argc) {
                fail("--report requires a file path");
            }
            config.report_path = argv[++i];
        } else if (arg == "--css-ext") {
            if (i + 1 >= argc) {
                fail("--css-ext requires a comma-separated list of extensions");
            }
            config.css_extensions.clear();
            parse_extensions(argv[++i], config.css_extensions);
        } else if (arg == "--code-ext") {
            if (i + 1 >= argc) {
                fail("--code-ext requires a comma-separated list of extensions");
            }
            config.code_extensions.clear();
            parse_extensions(argv[++i], config.code_extensions);
        } else if (arg == "--ignore") {
            if (i + 1 >= argc) {
                fail("--ignore requires a regular expression");
            }
            std::string pattern = argv[++i];
            if (pattern.empty()) {
                fail("--ignore requires a regular expression");
            }
            try {
                config.ignore_patterns.emplace_back(pattern);
            } catch (const std::regex_error&) {
                fail("invalid --ignore pattern: " + pattern);
            }
        } else if (!arg.empty() && arg.front() == '-') {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            std::cerr << "Use --help for usage information.\n";
            throw CliExit{2};
        } else if (config.src_directory == "src") {
            config.src_directory = arg;
        } else {
            fail("Unexpected argument: " + arg);
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
    std::cout << "    --cleanup           Remove unused CSS classes (keeps the first .backup)\n";
    std::cout << "    --no-report         Don't generate markdown report\n";
    std::cout << "    --css-ext <EXTS>    CSS file extensions (default: .css)\n";
    std::cout << "    --code-ext <EXTS>   Code file extensions (default: .tsx,.ts,.js,.jsx,.cshtml,.aspx)\n";
    std::cout << "    --ignore <REGEX>    Ignore class names fully matched by REGEX (repeatable)\n\n";
    std::cout << "EXAMPLES:\n";
    std::cout << "    css_checker                           # Analyze 'src' directory\n";
    std::cout << "    css_checker /path/to/project          # Analyze specific directory\n";
    std::cout << "    css_checker --verbose                 # Show detailed output\n";
    std::cout << "    css_checker --dry-run                 # Preview what would be cleaned\n";
    std::cout << "    css_checker --cleanup                 # Actually remove unused classes\n";
    std::cout << "    css_checker --src webapp --verbose    # Analyze 'webapp' directory with details\n";
    std::cout << "    css_checker --code-ext .ts,.tsx,.vue  # Custom code file extensions\n";
    std::cout << "    css_checker --ignore ^col- --ignore ^row$\n\n";
    std::cout << "DETECTION:\n";
    std::cout << "    Every configured code file is read once. A class counts as used when it\n";
    std::cout << "    appears in a class, className, or CssClass attribute, in class:btn or\n";
    std::cout << "    [class.btn] or @HostBinding(\"class.btn\"), in clsx/cn/classnames,\n";
    std::cout << "    in classList.add/toggle/contains/remove/replace, in setAttribute(\"class\", ...),\n";
    std::cout << "    in .addClass/.removeClass/.toggleClass, in .attr(\"class\") or .prop(\"className\"),\n";
    std::cout << "    in a className assignment,\n";
    std::cout << "    as a whole quoted token, as an\n";
    std::cout << "    object key such as clsx({ btn: on }) or className={{ active: on }}, or in\n";
    std::cout << "    @apply, @extend, or composes. Defining a selector does not count as a use.\n";
    std::cout << "    A computed value such as class=\"@Model.Css\", CssClass=\"<%# Item.Css %>\",\n";
    std::cout << "    className={styles.btn}, or :class=\"itemClass\" leaves every name without a\n";
    std::cout << "    literal use in place.\n";
    std::cout << "    A template such as icon--${name} or icon__${name}, \"icon--\" + name, or\n";
    std::cout << "    icon-@name marks that prefix as dynamic. Dynamic classes are reported\n";
    std::cout << "    and are not deleted.\n\n";
    std::cout << "CLEANUP:\n";
    std::cout << "    --cleanup deletes a rule only when a selector is exactly that class\n";
    std::cout << "    (optional pseudos allowed). .used .unused and .used.unused are kept.\n";
    std::cout << "    The first .backup beside a stylesheet is never overwritten.\n\n";
    std::cout << "EXIT CODES:\n";
    std::cout << "    0  No unused class has a selector that is exactly that class\n";
    std::cout << "    1  An unused class still has an exact selector\n";
    std::cout << "    2  Bad arguments, the files could not be analyzed, or cleanup failed\n";
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