#include "cli_parser.h"
#include "common.h"
#include "css_parser.h"
#include "file_scanner.h"
#include "report_generator.h"
#include "usage_detector.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace css_checker;

namespace {

int g_failures = 0;

void expect(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << file << ":" << line << "  " << expression << "\n";
        ++g_failures;
    }
}

#define EXPECT(condition) expect(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

void write_file(const fs::path& path, const std::string& contents) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

std::string read_file(const fs::path& path) {
    auto contents = utils::read_file(path.string());
    return contents ? *contents : std::string();
}

struct TempDir {
    fs::path path;

    TempDir() {
        auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path = fs::temp_directory_path() / ("css-checker-" + std::to_string(stamp));
        fs::create_directories(path);
    }

    ~TempDir() {
        std::error_code error;
        fs::remove_all(path, error);
    }
};

bool has_directory_named(const fs::path& path, const std::string& name) {
    for (const auto& part : path) {
        if (utils::to_lower(part.string()) == name) {
            return true;
        }
    }
    return false;
}

std::string capture_summary(const Config& config, const AnalysisResult& result, bool ran, bool ok) {
    std::ostringstream buffer;
    auto* previous = std::cout.rdbuf(buffer.rdbuf());
    ReportGenerator report(config);
    report.print_analysis_summary(result, ran, ok);
    std::cout.rdbuf(previous);
    return buffer.str();
}

const char* kAppCss = R"css(
/* .commented-out { color: pink; } */
.header { color: black; }
.btn { padding: 1px; }
.btn-primary { color: blue; }
.only-in-css { margin: 0; }
.icon--close { width: 1px; }
.live, .dead { color: red; }
.dead .live { color: green; }
.primary { color: orange; }
.lonely { color: purple; }
.ghost { color: gray; }
.alpha { float: left; }
.beta { float: right; }
.from-list { display: block; }
.banner { margin: 1px; }
.widget { margin: 2px; }
.card { margin: 3px; }
.twice { color: red; }
.twice { color: blue; }
)css";

const char* kAppTsx = R"tsx(
export function App() {
  const label = "primary school";
  const token = "lonely";
  // className="ghost"
  el.classList.add("from-list");
  const classes = clsx("alpha beta");
  return <div className="btn-primary header">{label}{token}{classes}</div>;
}
)tsx";

void test_extract_and_ignore() {
    TempDir dir;
    fs::path css = dir.path / "app.css";
    write_file(css, kAppCss);

    Config config;
    CssParser parser(config);
    auto classes = parser.extract_classes(css.string());

    auto has = [&](const std::string& name) {
        for (const auto& css_class : classes) {
            if (css_class.name == name) {
                return true;
            }
        }
        return false;
    };

    EXPECT(has("header"));
    EXPECT(has("btn"));
    EXPECT(has("btn-primary"));
    EXPECT(!has("commented-out"));

    int twice = 0;
    for (const auto& css_class : classes) {
        if (css_class.name == "twice") {
            ++twice;
        }
    }
    EXPECT(twice == 1);

    config.ignore_patterns.push_back(std::regex("^ghost$"));
    CssParser ignoring(config);
    auto filtered = ignoring.extract_classes(css.string());
    bool ghost = false;
    bool header = false;
    for (const auto& css_class : filtered) {
        ghost = ghost || css_class.name == "ghost";
        header = header || css_class.name == "header";
    }
    EXPECT(!ghost);
    EXPECT(header);
}

void test_usage_index() {
    TempDir dir;
    write_file(dir.path / "app.css", kAppCss);
    write_file(dir.path / "site.CSS", ".from-upper { color: red; }\n");
    write_file(dir.path / "node_modules" / "vendor.css", ".vendor-only { color: red; }\n");
    write_file(dir.path / "Node_Modules" / "other.css", ".also-vendor { color: red; }\n");
    write_file(dir.path / "bin" / "out.css", ".from-bin { color: red; }\n");
    write_file(dir.path / "obj" / "out.css", ".from-obj { color: red; }\n");
    write_file(dir.path / ".git" / "hidden.css", ".from-git { color: red; }\n");
    write_file(dir.path / "web" / "App.tsx", kAppTsx);
    write_file(dir.path / "web" / "Icon.tsx",
               "export const Icon = (name: string) => <i className={`icon--${name}`} />;\n");
    write_file(dir.path / "web" / "Page.cshtml", "<div class=\"banner\">Hello</div>\n");
    write_file(dir.path / "web" / "Form.aspx", "<asp:Label CssClass=\"widget\" runat=\"server\" />\n");
    write_file(dir.path / "web" / "Widget.vue", "<template><div class=\"card\"></div></template>\n");
    write_file(dir.path / "web" / "Skip.vue", "<div class=\"skipped\"></div>\n");

    Config config;
    config.src_directory = dir.path.string();
    config.code_extensions.push_back(".vue");

    FileScanner scanner(config);
    auto css_files = scanner.get_css_files();
    auto code_files = scanner.get_code_files();

    EXPECT(css_files.size() == 2);
    for (const auto& file : css_files) {
        EXPECT(!has_directory_named(file, "node_modules"));
        EXPECT(!has_directory_named(file, "bin"));
        EXPECT(!has_directory_named(file, "obj"));
        EXPECT(!has_directory_named(file, ".git"));
    }

    bool saw_upper = false;
    for (const auto& file : css_files) {
        saw_upper = saw_upper || utils::ends_with_ignore_case(file, "site.css");
    }
    EXPECT(saw_upper);

    bool saw_skip = false;
    bool saw_widget = false;
    for (const auto& file : code_files) {
        saw_skip = saw_skip || utils::ends_with_ignore_case(file, "skip.vue");
        saw_widget = saw_widget || utils::ends_with_ignore_case(file, "widget.vue");
    }
    EXPECT(saw_widget);
    EXPECT(saw_skip);

    Config defaults;
    defaults.src_directory = dir.path.string();
    FileScanner default_scanner(defaults);
    for (const auto& file : default_scanner.get_code_files()) {
        EXPECT(!utils::ends_with_ignore_case(file, ".vue"));
    }

    UsageDetector detector(config);
    UsageIndex index = detector.build_index(code_files);
    CssParser parser(config);

    auto disposition = [&](const std::string& name) {
        return detector.classify(index, name).disposition;
    };

    EXPECT(disposition("header") == ClassDisposition::Used);
    EXPECT(disposition("btn-primary") == ClassDisposition::Used);
    EXPECT(disposition("lonely") == ClassDisposition::Used);
    EXPECT(disposition("from-list") == ClassDisposition::Used);
    EXPECT(disposition("alpha") == ClassDisposition::Used);
    EXPECT(disposition("beta") == ClassDisposition::Used);
    EXPECT(disposition("banner") == ClassDisposition::Used);
    EXPECT(disposition("widget") == ClassDisposition::Used);
    EXPECT(disposition("card") == ClassDisposition::Used);
    EXPECT(disposition("skipped") == ClassDisposition::Used);

    EXPECT(disposition("btn") == ClassDisposition::Unused);
    EXPECT(disposition("primary") == ClassDisposition::Unused);
    EXPECT(disposition("ghost") == ClassDisposition::Unused);
    EXPECT(disposition("only-in-css") == ClassDisposition::Unused);
    EXPECT(disposition("from-upper") == ClassDisposition::Unused);
    EXPECT(disposition("vendor-only") == ClassDisposition::Unused);
    EXPECT(disposition("icon--close") == ClassDisposition::Unknown);

    fs::path copy = dir.path / "cleanup.css";
    write_file(copy, kAppCss);
    std::vector<std::string> unused_names;
    for (const auto& css_class : parser.extract_classes(copy.string())) {
        if (detector.classify(index, css_class.name).disposition == ClassDisposition::Unused) {
            unused_names.push_back(css_class.name);
        }
    }

    EXPECT(parser.cleanup_file(copy.string(), unused_names));
    std::string cleaned = read_file(copy);
    EXPECT(cleaned.find(".header") != std::string::npos);
    EXPECT(cleaned.find(".btn-primary") != std::string::npos);
    EXPECT(cleaned.find(".icon--close") != std::string::npos);
    EXPECT(cleaned.find(".lonely") != std::string::npos);
    EXPECT(cleaned.find(".dead .live") != std::string::npos);
    EXPECT(cleaned.find(".btn {") == std::string::npos);
    EXPECT(cleaned.find(".only-in-css") == std::string::npos);
    EXPECT(cleaned.find(".ghost") == std::string::npos);
    EXPECT(cleaned.find(".primary {") == std::string::npos);
    EXPECT(cleaned.find(".live, .dead") == std::string::npos);
    EXPECT(read_file(copy.string() + ".backup") == kAppCss);
}

void test_cleanup_rules() {
    Config config;
    CssParser parser(config);

    std::string shared = ".live, .dead { color: red; }\n";
    std::string shared_cleaned = parser.remove_unused_classes(shared, {"dead"});
    EXPECT(shared_cleaned.find(".live") != std::string::npos);
    EXPECT(shared_cleaned.find("color: red") != std::string::npos);
    EXPECT(shared_cleaned.find(".dead") == std::string::npos);

    std::string compound = ".dead .live { color: green; }\n.used.dead { color: blue; }\n";
    EXPECT(parser.remove_unused_classes(compound, {"dead"}) == compound);

    std::string hover = ".dead:hover { color: red; }\n.live { color: blue; }\n";
    std::string hover_cleaned = parser.remove_unused_classes(hover, {"dead"});
    EXPECT(hover_cleaned.find(".dead") == std::string::npos);
    EXPECT(hover_cleaned.find(".live") != std::string::npos);

    std::string nested = ".dead {\n  .live { color: blue; }\n}\n";
    EXPECT(parser.remove_unused_classes(nested, {"dead"}) == nested);

    std::string keyframes = "@keyframes spin {\n  from { opacity: 0; }\n}\n.spin { color: red; }\n";
    std::string keyframes_cleaned = parser.remove_unused_classes(keyframes, {"spin"});
    EXPECT(keyframes_cleaned.find("@keyframes spin") != std::string::npos);
    EXPECT(keyframes_cleaned.find(".spin") == std::string::npos);

    std::string media = "@media (min-width: 1px) {\n  .media-dead { color: red; }\n  .media-live { color: blue; }\n}\n";
    std::string media_cleaned = parser.remove_unused_classes(media, {"media-dead"});
    EXPECT(media_cleaned.find("@media") != std::string::npos);
    EXPECT(media_cleaned.find(".media-live") != std::string::npos);
    EXPECT(media_cleaned.find("media-dead") == std::string::npos);

    std::string empty_media = "@media (min-width: 1px) {\n  .media-dead { color: red; }\n}\n.kept { color: blue; }\n";
    std::string empty_media_cleaned = parser.remove_unused_classes(empty_media, {"media-dead"});
    EXPECT(empty_media_cleaned.find("@media") == std::string::npos);
    EXPECT(empty_media_cleaned.find(".kept") != std::string::npos);

    std::string comment = "/* .dead { color: red; } */\n.live { color: blue; }\n";
    std::string comment_cleaned = parser.remove_unused_classes(comment, {"dead"});
    EXPECT(comment_cleaned.find("/* .dead { color: red; } */") != std::string::npos);
    EXPECT(comment_cleaned.find(".live") != std::string::npos);

    std::string broken = ".live { color: blue; }\n.dead { color: red;\n";
    std::string broken_cleaned = parser.remove_unused_classes(broken, {"dead"});
    EXPECT(broken_cleaned.find(".live") != std::string::npos);
    EXPECT(broken_cleaned.find(".dead") != std::string::npos);
}

void test_backup_and_dry_run() {
    TempDir dir;
    fs::path css = dir.path / "app.css";
    std::string original = ".dead { color: red; }\n.live { color: blue; }\n";
    write_file(css, original);

    Config dry;
    dry.src_directory = dir.path.string();
    dry.dry_run = true;
    dry.cleanup_mode = true;
    CssParser dry_parser(dry);
    EXPECT(dry_parser.cleanup_file(css.string(), {"dead"}));
    EXPECT(read_file(css) == original);
    EXPECT(!fs::exists(css.string() + ".backup"));

    Config config;
    config.src_directory = dir.path.string();
    config.cleanup_mode = true;
    CssParser parser(config);
    EXPECT(parser.cleanup_file(css.string(), {"dead"}));
    EXPECT(read_file(css).find(".dead") == std::string::npos);
    EXPECT(read_file(css.string() + ".backup") == original);

    write_file(css.string() + ".backup", "SENTINEL");
    EXPECT(parser.cleanup_file(css.string(), {"live"}));
    EXPECT(read_file(css.string() + ".backup") == "SENTINEL");
    EXPECT(read_file(css).find(".live") == std::string::npos);

    EXPECT(!parser.cleanup_file((dir.path / "missing.css").string(), {"dead"}));
}

void test_summary_wording() {
    Config config;
    AnalysisResult result;
    result.total_css_files = 1;
    result.total_code_files = 1;
    result.unused_classes.push_back(UnusedClass{"dead", "app.css", ".dead { color: red; }"});

    std::string advice = capture_summary(config, result, false, true);
    EXPECT(advice.find("Run with --cleanup to delete rules whose selector is exactly an unused class.") != std::string::npos);
    EXPECT(advice.find("Use without --dry-run to perform cleanup") == std::string::npos);

    config.dry_run = true;
    std::string dry = capture_summary(config, result, true, true);
    EXPECT(dry.find("Apply changes with --cleanup and without --dry-run.") != std::string::npos);
    EXPECT(dry.find("Use without --dry-run to perform cleanup") == std::string::npos);

    config.dry_run = false;
    std::string failed = capture_summary(config, result, true, false);
    EXPECT(failed.find("Cleanup did not finish") != std::string::npos);

    AnalysisResult kept_only;
    kept_only.kept_classes.push_back(UnusedClass{"dead", "app.css", ".used .dead { color: red; }"});
    std::string kept = capture_summary(config, kept_only, false, true);
    EXPECT(kept.find("No exact-selector rule remains to remove.") != std::string::npos);
    EXPECT(kept.find("Run with --cleanup") == std::string::npos);
}

void test_selector_uses() {
    Config config;
    CssParser parser(config);

    auto both = parser.selector_uses(".dead { color: red; }\n.used .dead { color: blue; }\n");
    EXPECT(both["dead"].exact);
    EXPECT(both["dead"].complex);
    EXPECT(!both["used"].exact);
    EXPECT(both["used"].complex);

    auto nested = parser.selector_uses(".dead {\n  .live { color: blue; }\n}\n");
    EXPECT(!nested["dead"].exact);
    EXPECT(nested["dead"].complex);
    EXPECT(!nested["live"].exact);
    EXPECT(nested["live"].complex);

    auto hover = parser.selector_uses(".dead:hover { color: red; }\n");
    EXPECT(hover["dead"].exact);
    EXPECT(!hover["dead"].complex);

    auto media = parser.selector_uses(
        "@media (min-width: 1px) {\n  .media-dead { color: red; }\n  .used .media-live { color: blue; }\n}\n");
    EXPECT(media["media-dead"].exact);
    EXPECT(!media["media-dead"].complex);
    EXPECT(media["media-live"].complex);
    EXPECT(!media["media-live"].exact);
}

void test_style_references_and_ignore_flag() {
    TempDir dir;
    write_file(dir.path / "app.css",
               ".only-defined { color: red; }\n"
               ".card { @apply btn-primary hover:text-lg !important; }\n"
               ".child { @extend .parent, .other; }\n"
               ".panel { composes: shell, frame from \"./x.css\"; }\n"
               "/* @apply ghost */\n");
    write_file(dir.path / "App.tsx", "const css = `\n.x { @apply from-code; }\n`;\n");

    Config config;
    config.src_directory = dir.path.string();
    UsageDetector detector(config);
    UsageIndex index = detector.build_index({(dir.path / "App.tsx").string()}, {(dir.path / "app.css").string()});
    auto kind = [&](const std::string& name) { return detector.classify(index, name).disposition; };

    EXPECT(kind("btn-primary") == ClassDisposition::Used);
    EXPECT(kind("text-lg") == ClassDisposition::Used);
    EXPECT(kind("parent") == ClassDisposition::Used);
    EXPECT(kind("other") == ClassDisposition::Used);
    EXPECT(kind("shell") == ClassDisposition::Used);
    EXPECT(kind("frame") == ClassDisposition::Used);
    EXPECT(kind("from-code") == ClassDisposition::Used);
    EXPECT(kind("only-defined") == ClassDisposition::Unused);
    EXPECT(kind("ghost") == ClassDisposition::Unused);
    EXPECT(kind("card") == ClassDisposition::Unused);
    EXPECT(kind("important") == ClassDisposition::Unused);

    char arg0[] = "css_checker";
    char arg1[] = "--ignore";
    char arg2[] = "^col-";
    char arg3[] = "--ignore";
    char arg4[] = "^row$";
    char* argv[] = {arg0, arg1, arg2, arg3, arg4};
    CliParser cli;
    Config parsed = cli.parse(5, argv);
    EXPECT(parsed.ignore_patterns.size() == 2);
    EXPECT(std::regex_search(std::string("col-6"), parsed.ignore_patterns[0]));
    EXPECT(!std::regex_search(std::string("color"), parsed.ignore_patterns[0]));
    EXPECT(std::regex_search(std::string("row"), parsed.ignore_patterns[1]));
    EXPECT(!std::regex_search(std::string("row-gap"), parsed.ignore_patterns[1]));

    char bad0[] = "css_checker";
    char bad1[] = "--unknown";
    char* bad_argv[] = {bad0, bad1};
    bool threw = false;
    try {
        cli.parse(2, bad_argv);
    } catch (const CliExit& status) {
        threw = true;
        EXPECT(status.code == 2);
    }
    EXPECT(threw);
}

void test_computed_class_bindings() {
    Config config;
    UsageDetector detector(config);

    auto kind = [&](const UsageIndex& index, const std::string& name) {
        return detector.classify(index, name).disposition;
    };

    {
        TempDir dir;
        write_file(dir.path / "Page.cshtml",
                   "<div class=\"@Model.Css\"></div>\n"
                   "<span class=\"btn @Model.Extra\"></span>\n"
                   "<i class=@Model.Bare></i>\n");
        write_file(dir.path / "Form.aspx", "<asp:Label CssClass=\"<%# Item.Css %>\" runat=\"server\" />\n");
        UsageIndex index = detector.build_index(
            {(dir.path / "Page.cshtml").string(), (dir.path / "Form.aspx").string()});
        EXPECT(kind(index, "btn") == ClassDisposition::Used);
        EXPECT(kind(index, "banner") == ClassDisposition::Unknown);
        EXPECT(!index.opaque_file.empty());
        EXPECT(index.opaque_line > 0);
    }

    {
        TempDir dir;
        write_file(dir.path / "App.tsx",
                   "export const A = () => <div className={styles.btn} />;\n"
                   "export const B = () => <i className={on ? \"alpha\" : \"beta\"} />;\n");
        UsageIndex styles = detector.build_index({(dir.path / "App.tsx").string()});
        EXPECT(kind(styles, "btn") == ClassDisposition::Unknown);
        EXPECT(kind(styles, "alpha") == ClassDisposition::Used);
        EXPECT(kind(styles, "beta") == ClassDisposition::Used);
        EXPECT(kind(styles, "gamma") == ClassDisposition::Unknown);

        write_file(dir.path / "Choice.tsx",
                   "export const B = () => <i className={on ? \"alpha\" : \"beta\"} />;\n");
        UsageIndex choice = detector.build_index({(dir.path / "Choice.tsx").string()});
        EXPECT(kind(choice, "alpha") == ClassDisposition::Used);
        EXPECT(kind(choice, "beta") == ClassDisposition::Used);
        EXPECT(kind(choice, "gamma") == ClassDisposition::Unused);
        EXPECT(choice.opaque_file.empty());
    }

    {
        TempDir dir;
        write_file(dir.path / "Widget.vue",
                   "<b :class=\"itemClass\"></b>\n<b :class=\"on ? 'kept' : 'also'\"></b>\n");
        UsageIndex index = detector.build_index({(dir.path / "Widget.vue").string()});
        EXPECT(kind(index, "itemClass") == ClassDisposition::Unknown);
        EXPECT(kind(index, "kept") == ClassDisposition::Used);
        EXPECT(kind(index, "also") == ClassDisposition::Used);
        EXPECT(kind(index, "gamma") == ClassDisposition::Unknown);
    }

    {
        TempDir dir;
        write_file(dir.path / "Icon.tsx", "export const Icon = (name: string) => <i className={`icon--${name}`} />;\n");
        UsageIndex index = detector.build_index({(dir.path / "Icon.tsx").string()});
        EXPECT(kind(index, "icon--close") == ClassDisposition::Unknown);
        EXPECT(kind(index, "plain") == ClassDisposition::Unused);
        EXPECT(index.opaque_file.empty());
    }
}

void test_relative_path_respects_boundaries() {
    TempDir dir;
    fs::create_directories(dir.path / "src");
    fs::path other = dir.path / "src2" / "file.css";
    write_file(other, ".x { color: red; }\n");

    std::string relative = utils::get_relative_path(other.string(), (dir.path / "src").string());
    EXPECT(relative.find("src2") != std::string::npos);
    EXPECT(relative != "2/file.css");
    EXPECT(relative != "2\\file.css");
}

}

int main() {
    test_extract_and_ignore();
    test_usage_index();
    test_cleanup_rules();
    test_backup_and_dry_run();
    test_summary_wording();
    test_selector_uses();
    test_style_references_and_ignore_flag();
    test_computed_class_bindings();
    test_relative_path_respects_boundaries();

    if (g_failures != 0) {
        std::cerr << g_failures << " assertion(s) failed\n";
        return 1;
    }
    std::cout << "All checks passed\n";
    return 0;
}
