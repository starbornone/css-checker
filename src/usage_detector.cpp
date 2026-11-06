#include "usage_detector.h"

#include <cctype>
#include <string_view>

namespace css_checker {
namespace {

size_t skip_ws(const std::string& s, size_t i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    return i;
}

bool is_ident_char(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-';
}

struct TextSpan {
    std::string text;
    size_t end = 0;
};

TextSpan read_quoted(const std::string& s, size_t i) {
    char quote = s[i];
    std::string value;
    size_t j = i + 1;
    while (j < s.size()) {
        if (s[j] == '\\' && j + 1 < s.size()) {
            value.push_back(s[j + 1]);
            j += 2;
            continue;
        }
        if (s[j] == quote) {
            return {std::move(value), j + 1};
        }
        value.push_back(s[j]);
        ++j;
    }
    return {std::move(value), s.size()};
}

TextSpan read_template(const std::string& s, size_t i) {
    std::string body;
    size_t j = i + 1;
    while (j < s.size()) {
        if (s[j] == '\\' && j + 1 < s.size()) {
            body.push_back('\\');
            body.push_back(s[j + 1]);
            j += 2;
            continue;
        }
        if (s[j] == '`') {
            return {std::move(body), j + 1};
        }
        body.push_back(s[j]);
        ++j;
    }
    return {std::move(body), s.size()};
}

size_t skip_source_comment(const std::string& s, size_t i) {
    if (i >= s.size()) {
        return i;
    }
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '/' && (i == 0 || s[i - 1] != ':')) {
        while (i < s.size() && s[i] != '\n') {
            ++i;
        }
        return i;
    }
    if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '*') {
        i += 2;
        while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) {
            ++i;
        }
        return i + 1 < s.size() ? i + 2 : s.size();
    }
    if (s[i] == '<' && i + 3 < s.size() && s.compare(i, 4, "<!--") == 0) {
        i += 4;
        while (i + 2 < s.size() && s.compare(i, 3, "-->") != 0) {
            ++i;
        }
        return i + 2 < s.size() ? i + 3 : s.size();
    }
    if (s[i] == '@' && i + 1 < s.size() && s[i + 1] == '*') {
        i += 2;
        while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '@')) {
            ++i;
        }
        return i + 1 < s.size() ? i + 2 : s.size();
    }
    return i;
}

void add_token(UsageIndex& index,
               const std::string& token,
               const std::string& file_path,
               const std::string& content,
               size_t position,
               const std::string& usage_type) {
    if (!utils::is_class_token(token)) {
        return;
    }
    Usage usage;
    usage.file_path = file_path;
    usage.line_number = utils::find_line_number(content, position);
    usage.context = utils::get_line_context(content, usage.line_number);
    usage.usage_type = usage_type;
    index.usages[token].push_back(std::move(usage));
}

void add_ws_tokens(UsageIndex& index,
                   const std::string& value,
                   const std::string& file_path,
                   const std::string& content,
                   size_t position,
                   const std::string& usage_type) {
    size_t i = 0;
    while (i < value.size()) {
        if (std::isspace(static_cast<unsigned char>(value[i]))) {
            ++i;
            continue;
        }
        size_t start = i;
        while (i < value.size() && !std::isspace(static_cast<unsigned char>(value[i]))) {
            ++i;
        }
        add_token(index, value.substr(start, i - start), file_path, content, position, usage_type);
    }
}

void note_prefix_before(UsageIndex& index, const std::string& text, size_t brace_pos) {
    if (brace_pos == 0 || brace_pos > text.size()) {
        return;
    }
    size_t end = brace_pos;
    if (end > 0 && text[end - 1] == '$') {
        --end;
    }
    size_t start = end;
    while (start > 0 && is_ident_char(static_cast<unsigned char>(text[start - 1]))) {
        --start;
    }
    if (start == end || text[end - 1] != '-') {
        return;
    }
    std::string prefix = text.substr(start, end - start);
    if (utils::is_class_token(prefix) || (prefix.size() > 1 && prefix.back() == '-')) {
        index.dynamic_prefixes.insert(prefix);
    }
}

size_t skip_balanced(const std::string& text, size_t open, char open_ch, char close_ch) {
    if (open >= text.size() || text[open] != open_ch) {
        return open;
    }
    int depth = 0;
    for (size_t i = open; i < text.size(); ++i) {
        if (text[i] == open_ch) {
            ++depth;
        } else if (text[i] == close_ch) {
            --depth;
            if (depth == 0) {
                return i + 1;
            }
        }
    }
    return text.size();
}

void add_template_tokens(UsageIndex& index,
                         const std::string& body,
                         const std::string& file_path,
                         const std::string& content,
                         size_t position) {
    std::string part;
    auto flush = [&]() {
        add_ws_tokens(index, part, file_path, content, position, "template");
        part.clear();
    };

    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '\\' && i + 1 < body.size()) {
            part.push_back(body[i + 1]);
            ++i;
            continue;
        }
        if (body[i] == '$' && i + 1 < body.size() && body[i + 1] == '{') {
            note_prefix_before(index, body, i + 1);
            flush();
            i = skip_balanced(body, i + 1, '{', '}');
            if (i > 0) {
                --i;
            }
            continue;
        }
        part.push_back(body[i]);
    }
    flush();
}

void note_csharp_prefixes(UsageIndex& index, const std::string& body) {
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i] == '{') {
            note_prefix_before(index, body, i);
            i = skip_balanced(body, i, '{', '}');
            if (i > 0) {
                --i;
            }
        }
    }
}

bool match_keyword(const std::string& s, size_t i, std::string_view keyword, size_t& end) {
    if (i > 0 && is_ident_char(static_cast<unsigned char>(s[i - 1]))) {
        return false;
    }
    if (i + keyword.size() > s.size() || s.compare(i, keyword.size(), keyword) != 0) {
        return false;
    }
    size_t after = i + keyword.size();
    if (after < s.size() && is_ident_char(static_cast<unsigned char>(s[after]))) {
        return false;
    }
    end = after;
    return true;
}

size_t find_matching(const std::string& s, size_t open, char open_ch, char close_ch) {
    int depth = 0;
    for (size_t i = open; i < s.size();) {
        size_t comment = skip_source_comment(s, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (s[i] == '"' || s[i] == '\'') {
            i = read_quoted(s, i).end;
            continue;
        }
        if (s[i] == '`') {
            i = read_template(s, i).end;
            continue;
        }
        if (s[i] == open_ch) {
            ++depth;
            ++i;
            continue;
        }
        if (s[i] == close_ch) {
            --depth;
            if (depth == 0) {
                return i;
            }
            ++i;
            continue;
        }
        ++i;
    }
    return std::string::npos;
}

struct ExprParts {
    bool saw_literal = false;
    bool saw_code = false;
};

bool is_value_keyword(const std::string& ident) {
    return ident == "true" || ident == "false" || ident == "null" || ident == "undefined";
}

void note_opaque(UsageIndex& index, const std::string& file_path, const std::string& content, size_t position) {
    if (!index.opaque_file.empty()) {
        return;
    }
    index.opaque_file = file_path;
    index.opaque_line = utils::find_line_number(content, position);
}

bool markup_value_is_dynamic(const std::string& value) {
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '@' && i + 1 < value.size()) {
            unsigned char next = static_cast<unsigned char>(value[i + 1]);
            if (next == '@') {
                ++i;
                continue;
            }
            if (std::isalpha(next) || next == '_' || next == '(') {
                return true;
            }
        }
        if (value[i] == '<' && i + 1 < value.size() && value[i + 1] == '%') {
            return true;
        }
    }
    return false;
}

ExprParts scan_class_expression(UsageIndex& index,
                                const std::string& file_path,
                                const std::string& content,
                                size_t begin,
                                size_t end) {
    ExprParts parts;
    for (size_t i = begin; i < end && i < content.size();) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment < end ? comment : end;
            continue;
        }
        if (content[i] == '"' || content[i] == '\'') {
            TextSpan span = read_quoted(content, i);
            add_ws_tokens(index, span.text, file_path, content, i, "string");
            parts.saw_literal = true;
            i = span.end > i ? span.end : i + 1;
            continue;
        }
        if (content[i] == '`') {
            TextSpan span = read_template(content, i);
            add_template_tokens(index, span.text, file_path, content, i);
            parts.saw_literal = true;
            i = span.end > i ? span.end : i + 1;
            continue;
        }
        unsigned char c = static_cast<unsigned char>(content[i]);
        if (std::isalpha(c) || c == '_' || c == '$') {
            size_t start = i++;
            while (i < end && i < content.size()) {
                unsigned char ch = static_cast<unsigned char>(content[i]);
                if (!(is_ident_char(ch) || ch == '$')) {
                    break;
                }
                ++i;
            }
            if (!is_value_keyword(content.substr(start, i - start))) {
                parts.saw_code = true;
            }
            continue;
        }
        ++i;
    }
    return parts;
}

void note_expression(UsageIndex& index,
                     const std::string& file_path,
                     const std::string& content,
                     size_t position,
                     const ExprParts& parts) {
    if (parts.saw_code && !parts.saw_literal) {
        note_opaque(index, file_path, content, position);
    }
}

size_t try_class_attribute(UsageIndex& index,
                           const std::string& file_path,
                           const std::string& content,
                           size_t i) {
    size_t j = i;
    if (content[j] == '@') {
        ++j;
    }
    if (j >= content.size() || !(std::isalpha(static_cast<unsigned char>(content[j])) || content[j] == '_')) {
        return std::string::npos;
    }
    if (j > 0 && is_ident_char(static_cast<unsigned char>(content[j - 1])) && content[j - 1] != '@') {
        return std::string::npos;
    }

    size_t start = j;
    ++j;
    while (j < content.size() && is_ident_char(static_cast<unsigned char>(content[j]))) {
        ++j;
    }
    std::string keyword = utils::to_lower(content.substr(start, j - start));
    if (keyword != "class" && keyword != "classname" && keyword != "cssclass") {
        return std::string::npos;
    }
    bool bound = start > 0 && content[start - 1] == ':';

    j = skip_ws(content, j);
    if (j >= content.size() || content[j] != '=') {
        return std::string::npos;
    }
    j = skip_ws(content, j + 1);
    if (j >= content.size()) {
        return std::string::npos;
    }

    if (content[j] == '"' || content[j] == '\'') {
        TextSpan span = read_quoted(content, j);
        size_t value_end = span.end;
        if (value_end > j && value_end <= content.size() && content[value_end - 1] == content[j]) {
            --value_end;
        }
        if (bound) {
            note_expression(index, file_path, content, j,
                            scan_class_expression(index, file_path, content, j + 1, value_end));
        } else if (markup_value_is_dynamic(span.text)) {
            add_ws_tokens(index, span.text, file_path, content, j, "attribute");
            note_opaque(index, file_path, content, j);
        } else {
            add_ws_tokens(index, span.text, file_path, content, j, "attribute");
        }
        return span.end;
    }
    if (content[j] == '`') {
        TextSpan span = read_template(content, j);
        add_template_tokens(index, span.text, file_path, content, j);
        return span.end;
    }
    if (content[j] == '{') {
        size_t close = find_matching(content, j, '{', '}');
        if (close == std::string::npos) {
            return std::string::npos;
        }
        note_expression(index, file_path, content, j,
                        scan_class_expression(index, file_path, content, j + 1, close));
        return close + 1;
    }
    if (content[j] == '@' && j + 1 < content.size() && content[j + 1] != '@') {
        unsigned char next = static_cast<unsigned char>(content[j + 1]);
        if (std::isalpha(next) || next == '_' || next == '(') {
            note_opaque(index, file_path, content, j);
            if (next == '(') {
                size_t close = find_matching(content, j + 1, '(', ')');
                return close == std::string::npos ? content.size() : close + 1;
            }
            size_t k = j + 1;
            while (k < content.size() &&
                   (is_ident_char(static_cast<unsigned char>(content[k])) || content[k] == '.')) {
                ++k;
            }
            return k;
        }
    }
    if (content[j] == '<' && j + 1 < content.size() && content[j + 1] == '%') {
        note_opaque(index, file_path, content, j);
        size_t k = j + 2;
        while (k + 1 < content.size() && !(content[k] == '%' && content[k + 1] == '>')) {
            ++k;
        }
        return k + 1 < content.size() ? k + 2 : content.size();
    }
    return std::string::npos;
}

size_t try_class_call(UsageIndex& index,
                      const std::string& file_path,
                      const std::string& content,
                      size_t i) {
    size_t end = 0;
    bool matched = match_keyword(content, i, "clsx", end) ||
                   match_keyword(content, i, "cn", end) ||
                   match_keyword(content, i, "classnames", end);
    if (!matched && match_keyword(content, i, "classList", end)) {
        size_t j = skip_ws(content, end);
        if (j >= content.size() || content[j] != '.') {
            return std::string::npos;
        }
        j = skip_ws(content, j + 1);
        size_t method_end = 0;
        if (!(match_keyword(content, j, "add", method_end) ||
              match_keyword(content, j, "toggle", method_end) ||
              match_keyword(content, j, "contains", method_end) ||
              match_keyword(content, j, "remove", method_end))) {
            return std::string::npos;
        }
        end = method_end;
        matched = true;
    }
    if (!matched) {
        return std::string::npos;
    }

    size_t j = skip_ws(content, end);
    if (j >= content.size() || content[j] != '(') {
        return std::string::npos;
    }
    size_t close = find_matching(content, j, '(', ')');
    if (close == std::string::npos) {
        return std::string::npos;
    }
    note_expression(index, file_path, content, j,
                    scan_class_expression(index, file_path, content, j + 1, close));
    return close + 1;
}

void note_loose_prefixes(UsageIndex& index, const std::string& content) {
    for (size_t i = 0; i + 1 < content.size(); ++i) {
        if (content[i] == '`' ) {
            TextSpan span = read_template(content, i);
            for (size_t k = 0; k + 1 < span.text.size(); ++k) {
                if (span.text[k] == '$' && span.text[k + 1] == '{') {
                    note_prefix_before(index, span.text, k + 1);
                }
            }
            i = span.end > i ? span.end - 1 : i;
            continue;
        }
        if (content[i] == '$' && content[i + 1] == '"') {
            TextSpan span = read_quoted(content, i + 1);
            note_csharp_prefixes(index, span.text);
            i = span.end > i ? span.end - 1 : i;
        }
    }
}

bool keyword_at(const std::string& content, size_t i, std::string_view keyword) {
    if (i > 0) {
        unsigned char previous = static_cast<unsigned char>(content[i - 1]);
        if (std::isalnum(previous) || previous == '_' || previous == '-') {
            return false;
        }
    }
    if (i + keyword.size() > content.size() || content.compare(i, keyword.size(), keyword) != 0) {
        return false;
    }
    size_t after = i + keyword.size();
    if (after < content.size()) {
        unsigned char next = static_cast<unsigned char>(content[after]);
        if (std::isalnum(next) || next == '_' || next == '-') {
            return false;
        }
    }
    return true;
}

size_t consume_reference_list(UsageIndex& index,
                              const std::string& file_path,
                              const std::string& content,
                              size_t i,
                              const std::string& usage_type,
                              bool stop_at_from) {
    while (i < content.size()) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        unsigned char c = static_cast<unsigned char>(content[i]);
        if (content[i] == ';' || content[i] == '{' || content[i] == '}') {
            return i;
        }
        if (std::isspace(c) || content[i] == ',') {
            ++i;
            continue;
        }
        if (content[i] == '!') {
            if (i + 10 <= content.size() && content.compare(i + 1, 9, "important") == 0) {
                i += 10;
            } else {
                ++i;
            }
            continue;
        }
        if (content[i] == '"' || content[i] == '\'') {
            i = read_quoted(content, i).end;
            continue;
        }

        size_t start = i;
        while (i < content.size()) {
            unsigned char ch = static_cast<unsigned char>(content[i]);
            if (std::isspace(ch) || content[i] == ',' || content[i] == ';' || content[i] == '{' ||
                content[i] == '}' || content[i] == '!' || content[i] == '"' || content[i] == '\'') {
                break;
            }
            ++i;
        }
        std::string token = content.substr(start, i - start);
        if (stop_at_from && token == "from") {
            return i;
        }
        size_t colon = token.rfind(':');
        if (colon != std::string::npos) {
            token = token.substr(colon + 1);
        }
        if (!token.empty() && token.front() == '.') {
            token.erase(token.begin());
        }
        add_token(index, token, file_path, content, start, usage_type);
    }
    return content.size();
}

size_t consume_style_keyword(UsageIndex& index,
                             const std::string& file_path,
                             const std::string& content,
                             size_t i,
                             std::string_view keyword,
                             const std::string& usage_type,
                             bool stop_at_from) {
    size_t cursor = i + keyword.size();
    if (cursor < content.size() && content[cursor] == ':') {
        ++cursor;
    }
    size_t end = consume_reference_list(index, file_path, content, cursor, usage_type, stop_at_from);
    return end > i ? end : i + 1;
}

void index_style_references(UsageIndex& index, const std::string& file_path, const std::string& content) {
    for (size_t i = 0; i < content.size();) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment;
            continue;
        }
        if (content[i] == '"' || content[i] == '\'') {
            TextSpan span = read_quoted(content, i);
            i = span.end > i ? span.end : i + 1;
            continue;
        }
        if (keyword_at(content, i, "@apply")) {
            i = consume_style_keyword(index, file_path, content, i, "@apply", "apply", false);
            continue;
        }
        if (keyword_at(content, i, "@extend")) {
            i = consume_style_keyword(index, file_path, content, i, "@extend", "extend", false);
            continue;
        }
        if (keyword_at(content, i, "composes")) {
            i = consume_style_keyword(index, file_path, content, i, "composes", "composes", true);
            continue;
        }
        ++i;
    }
}

void index_content(UsageIndex& index, const std::string& file_path, const std::string& content) {
    index_style_references(index, file_path, content);
    for (size_t i = 0; i < content.size();) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment;
            continue;
        }

        size_t attribute_end = try_class_attribute(index, file_path, content, i);
        if (attribute_end != std::string::npos && attribute_end > i) {
            i = attribute_end;
            continue;
        }

        size_t call_end = try_class_call(index, file_path, content, i);
        if (call_end != std::string::npos && call_end > i) {
            i = call_end;
            continue;
        }

        if (content[i] == '"' || content[i] == '\'') {
            TextSpan span = read_quoted(content, i);
            if (utils::is_class_token(span.text)) {
                add_token(index, span.text, file_path, content, i, "string");
            }
            i = span.end > i ? span.end : i + 1;
            continue;
        }

        ++i;
    }

    note_loose_prefixes(index, content);
}

std::string matching_prefix(const UsageIndex& index, const std::string& class_name) {
    std::string found;
    for (const auto& prefix : index.dynamic_prefixes) {
        if (class_name.size() > prefix.size() &&
            class_name.compare(0, prefix.size(), prefix) == 0 &&
            prefix.size() > found.size()) {
            found = prefix;
        }
    }
    return found;
}

}

UsageDetector::UsageDetector(const Config& config) : config_(config) {}

UsageIndex UsageDetector::build_index(const std::vector<std::string>& code_files,
                                      const std::vector<std::string>& css_files) const {
    UsageIndex index;
    std::unordered_set<std::string> indexed;
    auto read_and = [&](const std::string& file_path, bool code_file) {
        if (!indexed.insert(file_path).second) {
            return;
        }
        auto content = utils::read_file(file_path);
        if (!content) {
            utils::colors::error("Warning: Could not open file: " +
                                 utils::get_relative_path(file_path, config_.src_directory));
            return;
        }
        if (config_.verbose) {
            utils::colors::log("Indexing: " + utils::get_relative_path(file_path, config_.src_directory),
                               utils::colors::BLUE);
        }
        if (code_file) {
            index_content(index, file_path, *content);
        } else {
            index_style_references(index, file_path, *content);
        }
    };

    for (const auto& file_path : code_files) {
        read_and(file_path, true);
    }
    for (const auto& file_path : css_files) {
        read_and(file_path, false);
    }
    return index;
}

ClassMatch UsageDetector::classify(const UsageIndex& index, const std::string& class_name) const {
    ClassMatch match;
    auto it = index.usages.find(class_name);
    if (it != index.usages.end() && !it->second.empty()) {
        match.disposition = ClassDisposition::Used;
        match.usages = it->second;
        return match;
    }

    std::string prefix = matching_prefix(index, class_name);
    if (!prefix.empty()) {
        match.disposition = ClassDisposition::Unknown;
        Usage usage;
        usage.context = "Dynamic prefix " + prefix;
        usage.usage_type = "dynamic";
        match.usages.push_back(std::move(usage));
        return match;
    }

    if (!index.opaque_file.empty()) {
        match.disposition = ClassDisposition::Unknown;
        Usage usage;
        usage.file_path = index.opaque_file;
        usage.line_number = index.opaque_line;
        usage.context = "Computed class binding";
        usage.usage_type = "dynamic";
        match.usages.push_back(std::move(usage));
        return match;
    }

    match.disposition = ClassDisposition::Unused;
    return match;
}

}
