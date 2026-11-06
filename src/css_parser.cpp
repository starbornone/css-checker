#include "css_parser.h"

#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace css_checker {
namespace {

struct NameSpan {
    std::string text;
    size_t end = 0;
};

bool is_escaped(const std::string& s, size_t index) {
    size_t slashes = 0;
    while (index > 0 && s[index - 1] == '\\') {
        ++slashes;
        --index;
    }
    return slashes % 2 == 1;
}

size_t skip_string(const std::string& s, size_t i) {
    if (i >= s.size()) {
        return i;
    }
    char quote = s[i];
    ++i;
    while (i < s.size()) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            i += 2;
            continue;
        }
        if (s[i] == quote) {
            return i + 1;
        }
        ++i;
    }
    return s.size();
}

size_t skip_comment(const std::string& s, size_t i) {
    if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '*') {
        i += 2;
        while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
            ++i;
        }
        if (i + 1 < s.size()) {
            return i + 2;
        }
        return s.size();
    }
    return i;
}

std::string strip_comments(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        size_t next = skip_comment(s, i);
        if (next != i) {
            i = next;
            continue;
        }
        if (s[i] == '"' || s[i] == '\'') {
            size_t end = skip_string(s, i);
            out.append(s, i, end - i);
            i = end;
            continue;
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

NameSpan read_class_name(const std::string& s, size_t i) {
    size_t start = i;
    if (i >= s.size()) {
        return {{}, i};
    }
    unsigned char first = static_cast<unsigned char>(s[i]);
    if (first == '-') {
        if (i + 1 >= s.size()) {
            return {{}, i};
        }
        unsigned char next = static_cast<unsigned char>(s[i + 1]);
        if (!(std::isalpha(next) || next == '_')) {
            return {{}, i};
        }
        i += 2;
    } else if (!(std::isalpha(first) || first == '_')) {
        return {{}, i};
    } else {
        ++i;
    }

    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (!(std::isalnum(c) || c == '_' || c == '-')) {
            break;
        }
        ++i;
    }
    return {s.substr(start, i - start), i};
}

struct BraceMatch {
    size_t close = 0;
    bool nested = false;
    bool ok = false;
};

BraceMatch match_brace(const std::string& s, size_t open) {
    int depth = 0;
    bool nested = false;
    for (size_t i = open; i < s.size();) {
        size_t comment = skip_comment(s, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (s[i] == '"' || s[i] == '\'') {
            i = skip_string(s, i);
            continue;
        }
        if (s[i] == '{') {
            ++depth;
            if (depth > 1) {
                nested = true;
            }
            ++i;
            continue;
        }
        if (s[i] == '}') {
            --depth;
            if (depth == 0) {
                return BraceMatch{i, nested, true};
            }
            ++i;
            continue;
        }
        ++i;
    }
    return BraceMatch{s.size(), nested, false};
}

bool starts_with_keyword(const std::string& text, const std::string& keyword) {
    if (text.size() < keyword.size() || text.compare(0, keyword.size(), keyword) != 0) {
        return false;
    }
    if (text.size() == keyword.size()) {
        return true;
    }
    unsigned char next = static_cast<unsigned char>(text[keyword.size()]);
    return !(std::isalnum(next) || next == '_' || next == '-');
}

bool is_conditional_at_rule(const std::string& prelude) {
    std::string text = utils::to_lower(utils::trim(strip_comments(prelude)));
    return starts_with_keyword(text, "@media") ||
           starts_with_keyword(text, "@supports") ||
           starts_with_keyword(text, "@container") ||
           starts_with_keyword(text, "@layer") ||
           starts_with_keyword(text, "@document") ||
           starts_with_keyword(text, "@scope");
}

bool is_other_at_rule(const std::string& prelude) {
    std::string text = utils::trim(strip_comments(prelude));
    return !text.empty() && text[0] == '@';
}

std::vector<std::string> split_selectors(const std::string& text) {
    std::vector<std::string> parts;
    std::string current;
    int depth = 0;
    for (char c : text) {
        if (c == '(') {
            ++depth;
            current.push_back(c);
            continue;
        }
        if (c == ')') {
            if (depth > 0) {
                --depth;
            }
            current.push_back(c);
            continue;
        }
        if (c == ',' && depth == 0) {
            parts.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    parts.push_back(current);
    return parts;
}

size_t skip_pseudo_args(const std::string& sel, size_t i) {
    int depth = 0;
    for (; i < sel.size(); ++i) {
        if (sel[i] == '(') {
            ++depth;
            continue;
        }
        if (sel[i] == ')') {
            --depth;
            if (depth == 0) {
                return i + 1;
            }
            continue;
        }
        if (sel[i] == '.' || sel[i] == '{' || sel[i] == '}') {
            return std::string::npos;
        }
    }
    return depth == 0 ? i : std::string::npos;
}

// A selector is removable only when it is exactly one class, optionally with
// pseudos. Compound selectors such as ".used .unused" or ".used.unused" stay,
// because deleting them would change which elements match.
bool is_exact_class_selector(const std::string& selector, std::string& name) {
    std::string sel = utils::trim(selector);
    if (sel.empty() || sel[0] != '.') {
        return false;
    }
    NameSpan span = read_class_name(sel, 1);
    if (span.text.empty() || !utils::is_class_token(span.text)) {
        return false;
    }
    name = span.text;
    size_t i = span.end;
    while (i < sel.size()) {
        if (sel[i] != ':') {
            return false;
        }
        ++i;
        if (i < sel.size() && sel[i] == ':') {
            ++i;
        }
        if (i >= sel.size() || !(std::isalpha(static_cast<unsigned char>(sel[i])) || sel[i] == '_')) {
            return false;
        }
        while (i < sel.size()) {
            unsigned char c = static_cast<unsigned char>(sel[i]);
            if (!(std::isalnum(c) || c == '_' || c == '-')) {
                break;
            }
            ++i;
        }
        if (i < sel.size() && sel[i] == '(') {
            i = skip_pseudo_args(sel, i);
            if (i == std::string::npos) {
                return false;
            }
        }
    }
    return true;
}

std::string leading_trivia(const std::string& prelude, size_t& selector_start) {
    std::string leading;
    size_t i = 0;
    while (i < prelude.size()) {
        unsigned char c = static_cast<unsigned char>(prelude[i]);
        if (std::isspace(c)) {
            leading.push_back(prelude[i]);
            ++i;
            continue;
        }
        size_t comment = skip_comment(prelude, i);
        if (comment != i) {
            leading.append(prelude, i, comment - i);
            i = comment;
            continue;
        }
        break;
    }
    selector_start = i;
    return leading;
}

std::string rewrite_style_rule(const std::string& prelude,
                               const std::string& inner,
                               const std::unordered_set<std::string>& unused) {
    size_t selector_start = 0;
    std::string leading = leading_trivia(prelude, selector_start);
    std::string selector_text = utils::trim(strip_comments(prelude.substr(selector_start)));
    if (selector_text.empty()) {
        return prelude + "{" + inner + "}";
    }

    std::vector<std::string> kept;
    bool removed_any = false;
    for (const std::string& part : split_selectors(selector_text)) {
        std::string class_name;
        if (is_exact_class_selector(part, class_name) && unused.count(class_name) != 0) {
            removed_any = true;
            continue;
        }
        std::string trimmed = utils::trim(part);
        if (!trimmed.empty()) {
            kept.push_back(trimmed);
        }
    }

    if (!removed_any) {
        return prelude + "{" + inner + "}";
    }
    if (kept.empty()) {
        return "";
    }

    std::string joined;
    for (size_t i = 0; i < kept.size(); ++i) {
        if (i > 0) {
            joined += ", ";
        }
        joined += kept[i];
    }
    return leading + joined + " {" + inner + "}";
}

void mark_classes_complex(const std::string& text, std::unordered_map<std::string, SelectorUse>& uses) {
    for (size_t i = 0; i < text.size();) {
        size_t comment = skip_comment(text, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (text[i] == '"' || text[i] == '\'') {
            i = skip_string(text, i);
            continue;
        }
        if (text[i] == '.' && !is_escaped(text, i)) {
            NameSpan name = read_class_name(text, i + 1);
            if (!name.text.empty() && utils::is_class_token(name.text)) {
                uses[name.text].complex = true;
                i = name.end;
                continue;
            }
        }
        ++i;
    }
}

void collect_uses_impl(const std::string& css, std::unordered_map<std::string, SelectorUse>& uses) {
    size_t segment_start = 0;
    for (size_t i = 0; i < css.size();) {
        size_t comment = skip_comment(css, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (css[i] == '"' || css[i] == '\'') {
            i = skip_string(css, i);
            continue;
        }
        if (css[i] != '{') {
            ++i;
            continue;
        }

        std::string prelude = css.substr(segment_start, i - segment_start);
        BraceMatch brace = match_brace(css, i);
        if (!brace.ok) {
            mark_classes_complex(css.substr(segment_start), uses);
            return;
        }

        std::string inner = css.substr(i + 1, brace.close - i - 1);
        if (is_conditional_at_rule(prelude)) {
            collect_uses_impl(inner, uses);
        } else if (brace.nested || is_other_at_rule(prelude)) {
            mark_classes_complex(prelude + "{" + inner + "}", uses);
        } else {
            size_t selector_start = 0;
            leading_trivia(prelude, selector_start);
            std::string selector_text = utils::trim(strip_comments(prelude.substr(selector_start)));
            for (const std::string& part : split_selectors(selector_text)) {
                std::string class_name;
                if (is_exact_class_selector(part, class_name)) {
                    uses[class_name].exact = true;
                } else if (!utils::trim(part).empty()) {
                    mark_classes_complex(part, uses);
                }
            }
        }

        i = brace.close + 1;
        segment_start = i;
    }
}

std::string remove_unused_impl(const std::string& css,
                               const std::unordered_set<std::string>& unused) {
    std::string result;
    size_t segment_start = 0;
    for (size_t i = 0; i < css.size();) {
        size_t comment = skip_comment(css, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (css[i] == '"' || css[i] == '\'') {
            i = skip_string(css, i);
            continue;
        }
        if (css[i] != '{') {
            ++i;
            continue;
        }

        std::string prelude = css.substr(segment_start, i - segment_start);
        BraceMatch brace = match_brace(css, i);
        if (!brace.ok) {
            result += css.substr(segment_start);
            return result;
        }

        std::string inner = css.substr(i + 1, brace.close - i - 1);
        if (is_conditional_at_rule(prelude)) {
            std::string cleaned = remove_unused_impl(inner, unused);
            if (!utils::trim(cleaned).empty()) {
                result += prelude + "{" + cleaned + "}";
            }
        } else if (brace.nested || is_other_at_rule(prelude)) {
            result += prelude + "{" + inner + "}";
        } else {
            result += rewrite_style_rule(prelude, inner, unused);
        }

        i = brace.close + 1;
        segment_start = i;
    }
    result += css.substr(segment_start);
    return result;
}

}

CssParser::CssParser(const Config& config) : config_(config) {}

std::vector<CssClass> CssParser::extract_classes(const std::string& css_file_path) const {
    auto content = utils::read_file(css_file_path);
    if (!content) {
        utils::colors::error("Warning: Could not open CSS file: " + css_file_path);
        return {};
    }
    return extract_classes(*content, css_file_path);
}

std::vector<CssClass> CssParser::extract_classes(const std::string& css_content,
                                                 const std::string& css_file_path) const {
    std::vector<CssClass> classes;
    std::unordered_set<std::string> seen;
    for (size_t i = 0; i < css_content.size();) {
        size_t comment = skip_comment(css_content, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (css_content[i] == '"' || css_content[i] == '\'') {
            i = skip_string(css_content, i);
            continue;
        }
        if (css_content[i] == '.' && !is_escaped(css_content, i)) {
            NameSpan name = read_class_name(css_content, i + 1);
            if (!name.text.empty() && utils::is_class_token(name.text)) {
                if (!should_ignore_class(name.text) && seen.insert(name.text).second) {
                    CssClass css_class;
                    css_class.name = name.text;
                    css_class.file_path = css_file_path;
                    css_class.line_number = utils::find_line_number(css_content, i);
                    css_class.context = utils::get_line_context(css_content, css_class.line_number);
                    classes.push_back(std::move(css_class));
                }
                i = name.end;
                continue;
            }
        }
        ++i;
    }
    return classes;
}

std::unordered_map<std::string, SelectorUse> CssParser::selector_uses(const std::string& css_content) const {
    std::unordered_map<std::string, SelectorUse> uses;
    collect_uses_impl(css_content, uses);
    return uses;
}

std::string CssParser::remove_unused_classes(const std::string& css_content,
                                             const std::vector<std::string>& unused_classes) const {
    std::unordered_set<std::string> unused(unused_classes.begin(), unused_classes.end());
    return remove_unused_impl(css_content, unused);
}

bool CssParser::cleanup_file(const std::string& css_file,
                             const std::vector<std::string>& unused_classes) const {
    if (config_.dry_run) {
        utils::colors::log("    [DRY RUN] Would remove " + std::to_string(unused_classes.size()) +
                               " unused classes from " +
                               utils::get_relative_path(css_file, config_.src_directory),
                           utils::colors::YELLOW);
        return true;
    }

    std::string backup_path = css_file + ".backup";
    try {
        if (std::filesystem::exists(backup_path)) {
            utils::colors::log("    Keeping existing backup: " +
                                   utils::get_relative_path(backup_path, config_.src_directory),
                               utils::colors::YELLOW);
        } else {
            std::filesystem::copy_file(css_file, backup_path);
            utils::colors::log("    Backup created: " +
                                   utils::get_relative_path(backup_path, config_.src_directory),
                               utils::colors::YELLOW);
        }
    } catch (const std::exception& e) {
        utils::colors::error("Error creating backup for " + css_file + ": " + e.what());
        return false;
    }

    auto content = utils::read_file(css_file);
    if (!content) {
        utils::colors::error("Error: Could not open CSS file for cleanup: " + css_file);
        return false;
    }

    std::string cleaned = remove_unused_classes(*content, unused_classes);
    if (cleaned == *content) {
        utils::colors::log("    No exact-selector rules to remove in " +
                               utils::get_relative_path(css_file, config_.src_directory),
                           utils::colors::YELLOW);
        return true;
    }

    std::filesystem::path destination(css_file);
    std::filesystem::path temporary = destination;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out) {
            utils::colors::error("Error: Could not write temporary CSS file: " + temporary.string());
            return false;
        }
        out.write(cleaned.data(), static_cast<std::streamsize>(cleaned.size()));
        out.flush();
        if (!out) {
            utils::colors::error("Error: Could not finish writing " + temporary.string());
            std::error_code remove_error;
            std::filesystem::remove(temporary, remove_error);
            return false;
        }
    }

    std::error_code rename_error;
    std::filesystem::rename(temporary, destination, rename_error);
    if (rename_error) {
        std::error_code remove_error;
        std::filesystem::remove(destination, remove_error);
        std::filesystem::rename(temporary, destination, rename_error);
        if (rename_error) {
            utils::colors::error("Error: Could not replace " + css_file + " (" + rename_error.message() +
                                 "). Cleaned copy: " + temporary.string());
            return false;
        }
    }

    utils::colors::log("    Removed exact selectors in " +
                           utils::get_relative_path(css_file, config_.src_directory),
                       utils::colors::GREEN);
    return true;
}

bool CssParser::should_ignore_class(const std::string& class_name) const {
    for (const auto& pattern : config_.ignore_patterns) {
        if (std::regex_search(class_name, pattern)) {
            return true;
        }
    }
    return false;
}

}
