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
    if (start == end || (text[end - 1] != '-' && text[end - 1] != '_')) {
        return;
    }
    std::string prefix = text.substr(start, end - start);
    if (prefix.size() >= 2 && utils::is_class_token(prefix)) {
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
    bool saw_unresolved = false;
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

size_t skip_gap(const std::string& content, size_t i, size_t end) {
    while (i < end && i < content.size()) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment < end ? comment : end;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(content[i]))) {
            ++i;
            continue;
        }
        break;
    }
    return i;
}

bool followed_by_colon(const std::string& content, size_t i, size_t end) {
    size_t next = skip_gap(content, i, end);
    return next < end && next < content.size() && content[next] == ':';
}

bool class_prefix_suffix(const std::string& text, std::string& prefix) {
    if (text.empty() || std::isspace(static_cast<unsigned char>(text.back()))) {
        return false;
    }
    size_t start = text.size();
    while (start > 0 && !std::isspace(static_cast<unsigned char>(text[start - 1]))) {
        --start;
    }
    prefix = text.substr(start);
    if (prefix.size() < 2 || (prefix.back() != '-' && prefix.back() != '_')) {
        return false;
    }
    return utils::is_class_token(prefix);
}

void add_static_head(UsageIndex& index,
                     const std::string& file_path,
                     const std::string& content,
                     size_t position,
                     const std::string& pending,
                     const std::string& prefix) {
    if (pending.size() < prefix.size()) {
        return;
    }
    add_ws_tokens(index, pending.substr(0, pending.size() - prefix.size()), file_path, content, position, "attribute");
}

bool interpolation_is_arbitrary(UsageIndex& index,
                                const std::string& file_path,
                                const std::string& content,
                                size_t position,
                                const std::string& body) {
    bool arbitrary = false;
    std::string pending;
    for (size_t i = 0; i < body.size();) {
        if (body[i] == '{') {
            std::string prefix;
            if (class_prefix_suffix(pending, prefix)) {
                add_static_head(index, file_path, content, position, pending, prefix);
                index.dynamic_prefixes.insert(std::move(prefix));
            } else {
                add_ws_tokens(index, pending, file_path, content, position, "attribute");
                arbitrary = true;
            }
            pending.clear();
            size_t next = skip_balanced(body, i, '{', '}');
            i = next > i ? next : i + 1;
            continue;
        }
        pending.push_back(body[i]);
        ++i;
    }
    if (!pending.empty() && pending.front() != '-' && !std::isspace(static_cast<unsigned char>(pending.front()))) {
        add_ws_tokens(index, pending, file_path, content, position, "attribute");
    } else if (!pending.empty() && std::isspace(static_cast<unsigned char>(pending.front()))) {
        add_ws_tokens(index, pending, file_path, content, position, "attribute");
    }
    return arbitrary;
}

size_t match_markup_paren(const std::string& text, size_t open) {
    int depth = 0;
    for (size_t i = open; i < text.size(); ++i) {
        if (text[i] == '"' || text[i] == '\'') {
            size_t next = read_quoted(text, i).end;
            i = next > i ? next - 1 : i;
            continue;
        }
        if (text[i] == '(') {
            ++depth;
        } else if (text[i] == ')') {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    return std::string::npos;
}

TextSpan read_markup_quoted(const std::string& s, size_t i) {
    char quote = s[i];
    std::string value;
    size_t j = i + 1;
    while (j < s.size()) {
        if (s[j] == '@' && j + 1 < s.size() && s[j + 1] == '@') {
            value.append("@@");
            j += 2;
            continue;
        }
        if (s[j] == '@' && j + 1 < s.size() && s[j + 1] == '(') {
            size_t close = match_markup_paren(s, j + 1);
            size_t end = close == std::string::npos ? s.size() : close + 1;
            value.append(s, j, end - j);
            j = end;
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

bool only_interpolated_string(const std::string& body, std::string& literal) {
    size_t i = 0;
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) {
        ++i;
    }
    if (i < body.size() && body[i] == '$') {
        ++i;
    }
    if (i >= body.size() || (body[i] != '"' && body[i] != '\'')) {
        return false;
    }
    TextSpan span = read_quoted(body, i);
    i = span.end;
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) {
        ++i;
    }
    if (i != body.size()) {
        return false;
    }
    literal = std::move(span.text);
    return true;
}

bool markup_value_is_arbitrary(UsageIndex& index,
                               const std::string& file_path,
                               const std::string& content,
                               size_t position,
                               const std::string& value) {
    bool arbitrary = false;
    std::string pending;
    auto continue_hole = [&]() {
        std::string prefix;
        if (class_prefix_suffix(pending, prefix)) {
            index.dynamic_prefixes.insert(prefix);
            add_static_head(index, file_path, content, position, pending, prefix);
        } else {
            add_ws_tokens(index, pending, file_path, content, position, "attribute");
            arbitrary = true;
        }
        pending.clear();
    };

    for (size_t i = 0; i < value.size();) {
        if (value[i] == '@' && i + 1 < value.size() && value[i + 1] == '@') {
            pending.push_back('@');
            i += 2;
            continue;
        }
        if (value[i] == '@' && i + 1 < value.size() && value[i + 1] == '(') {
            size_t close = match_markup_paren(value, i + 1);
            std::string body = close == std::string::npos ? std::string() : value.substr(i + 2, close - (i + 2));
            std::string literal;
            bool pure = close != std::string::npos && only_interpolated_string(body, literal);
            if (!pending.empty() && !std::isspace(static_cast<unsigned char>(pending.back()))) {
                continue_hole();
            } else {
                add_ws_tokens(index, pending, file_path, content, position, "attribute");
                pending.clear();
            }
            if (!pure || interpolation_is_arbitrary(index, file_path, content, position, literal)) {
                arbitrary = true;
            }
            i = close == std::string::npos ? value.size() : close + 1;
            continue;
        }
        if (value[i] == '@' && i + 1 < value.size()) {
            unsigned char next = static_cast<unsigned char>(value[i + 1]);
            if (std::isalpha(next) || next == '_') {
                continue_hole();
                i += 2;
                while (i < value.size() &&
                       (is_ident_char(static_cast<unsigned char>(value[i])) || value[i] == '.')) {
                    ++i;
                }
                continue;
            }
        }
        if (value[i] == '<' && i + 1 < value.size() && value[i + 1] == '%') {
            continue_hole();
            i += 2;
            while (i + 1 < value.size() && !(value[i] == '%' && value[i + 1] == '>')) {
                ++i;
            }
            i = i + 1 < value.size() ? i + 2 : value.size();
            continue;
        }
        pending.push_back(value[i]);
        ++i;
    }
    add_ws_tokens(index, pending, file_path, content, position, "attribute");
    return arbitrary;
}

ExprParts scan_class_expression(UsageIndex& index,
                                const std::string& file_path,
                                const std::string& content,
                                size_t begin,
                                size_t end) {
    ExprParts parts;
    char previous = 0;
    bool hyphen_operand = false;
    bool code_operand = false;
    std::string hyphen_prefix;
    for (size_t i = begin; i < end && i < content.size();) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment < end ? comment : end;
            continue;
        }
        unsigned char c = static_cast<unsigned char>(content[i]);
        if (std::isspace(c)) {
            ++i;
            continue;
        }
        if (content[i] == '"' || content[i] == '\'') {
            TextSpan span = read_quoted(content, i);
            add_ws_tokens(index, span.text, file_path, content, i, "string");
            parts.saw_literal = true;
            if (previous == '+' && code_operand && !hyphen_operand) {
                parts.saw_unresolved = true;
            }
            std::string prefix;
            if (class_prefix_suffix(span.text, prefix)) {
                hyphen_operand = true;
                hyphen_prefix = std::move(prefix);
                code_operand = false;
            } else {
                hyphen_operand = false;
                code_operand = false;
            }
            previous = 'w';
            i = span.end > i ? span.end : i + 1;
            continue;
        }
        if (content[i] == '`') {
            TextSpan span = read_template(content, i);
            add_template_tokens(index, span.text, file_path, content, i);
            parts.saw_literal = true;
            hyphen_operand = false;
            code_operand = false;
            previous = 'w';
            i = span.end > i ? span.end : i + 1;
            continue;
        }
        if (std::isalpha(c) || c == '_' || c == '$') {
            size_t start = i++;
            while (i < end && i < content.size()) {
                unsigned char ch = static_cast<unsigned char>(content[i]);
                if (!(is_ident_char(ch) || ch == '$')) {
                    break;
                }
                ++i;
            }
            std::string ident = content.substr(start, i - start);
            bool key = (previous == '{' || previous == ',') && followed_by_colon(content, i, end);
            if (key && !is_value_keyword(ident) && utils::is_class_token(ident)) {
                add_token(index, ident, file_path, content, start, "object");
                parts.saw_literal = true;
                hyphen_operand = false;
                code_operand = false;
            } else if (!is_value_keyword(ident)) {
                parts.saw_code = true;
                if (!hyphen_operand) {
                    code_operand = true;
                }
            }
            previous = 'w';
            continue;
        }
        if (content[i] == '+') {
            if (hyphen_operand) {
                index.dynamic_prefixes.insert(hyphen_prefix);
            } else {
                size_t next = skip_gap(content, i + 1, end);
                bool right_is_string = next < end && next < content.size() &&
                                       (content[next] == '"' || content[next] == '\'' || content[next] == '`');
                if (!right_is_string || code_operand) {
                    parts.saw_unresolved = true;
                }
            }
            previous = '+';
            ++i;
            continue;
        }
        if (content[i] == '.' && i + 2 < end && i + 2 < content.size() && content[i + 1] == '.' &&
            content[i + 2] == '.') {
            parts.saw_unresolved = true;
            previous = '.';
            i += 3;
            continue;
        }
        if ((previous == '{' || previous == ',') && content[i] == '[') {
            parts.saw_unresolved = true;
        }
        hyphen_operand = false;
        code_operand = false;
        previous = content[i];
        ++i;
    }
    return parts;
}

void note_expression(UsageIndex& index,
                     const std::string& file_path,
                     const std::string& content,
                     size_t position,
                     const ExprParts& parts) {
    if (parts.saw_unresolved || (parts.saw_code && !parts.saw_literal)) {
        note_opaque(index, file_path, content, position);
    }
}

size_t skip_attribute_value(const std::string& content, size_t equals) {
    size_t j = skip_ws(content, equals);
    if (j >= content.size() || content[j] != '=') {
        return j;
    }
    j = skip_ws(content, j + 1);
    if (j >= content.size()) {
        return j;
    }
    if (content[j] == '"' || content[j] == '\'') {
        return read_markup_quoted(content, j).end;
    }
    if (content[j] == '{') {
        size_t close = find_matching(content, j, '{', '}');
        return close == std::string::npos ? content.size() : close + 1;
    }
    while (j < content.size() && !std::isspace(static_cast<unsigned char>(content[j])) && content[j] != '>' &&
           content[j] != '/') {
        ++j;
    }
    return j;
}

size_t finish_class_directive(UsageIndex& index,
                              const std::string& file_path,
                              const std::string& content,
                              size_t name_at,
                              bool expect_bracket) {
    size_t j = skip_ws(content, name_at);
    if (j >= content.size() || !(std::isalpha(static_cast<unsigned char>(content[j])) || content[j] == '_')) {
        return std::string::npos;
    }
    size_t start = j++;
    while (j < content.size() && is_ident_char(static_cast<unsigned char>(content[j]))) {
        ++j;
    }
    add_token(index, content.substr(start, j - start), file_path, content, start, "directive");
    if (expect_bracket) {
        j = skip_ws(content, j);
        if (j < content.size() && content[j] == ']') {
            ++j;
        }
    }
    j = skip_ws(content, j);
    if (j < content.size() && content[j] == '=') {
        return skip_attribute_value(content, j);
    }
    return j;
}

size_t end_of_js_expression(const std::string& content, size_t i);

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
    if (keyword != "class" && keyword != "classname" && keyword != "cssclass" && keyword != "ngclass") {
        return std::string::npos;
    }
    bool bound = start > 0 && content[start - 1] == ':';

    j = skip_ws(content, j);
    if (keyword == "class" && j < content.size() && content[j] == ':') {
        return finish_class_directive(index, file_path, content, j + 1, false);
    }
    if (keyword == "class" && start > 0 && content[start - 1] == '[' && j < content.size() && content[j] == '.') {
        return finish_class_directive(index, file_path, content, j + 1, true);
    }
    if (j < content.size() && content[j] == ']' &&
        (keyword == "ngclass" || (keyword == "class" && start > 0 && content[start - 1] == '['))) {
        ++j;
        j = skip_ws(content, j);
        bound = true;
    }
    bool js_assignment = keyword == "classname" || keyword == "cssclass" || (start > 0 && content[start - 1] == '.');
    if (js_assignment && j + 1 < content.size() && content[j] == '+' && content[j + 1] == '=') {
        ++j;
    }
    if (j >= content.size() || content[j] != '=') {
        return std::string::npos;
    }
    j = skip_ws(content, j + 1);
    if (j >= content.size()) {
        return std::string::npos;
    }

    if (content[j] == '"' || content[j] == '\'') {
        TextSpan span = read_markup_quoted(content, j);
        size_t value_end = span.end;
        if (value_end > j && value_end <= content.size() && content[value_end - 1] == content[j]) {
            --value_end;
        }
        if (bound) {
            note_expression(index, file_path, content, j,
                            scan_class_expression(index, file_path, content, j + 1, value_end));
        } else if (markup_value_is_dynamic(span.text)) {
            if (markup_value_is_arbitrary(index, file_path, content, j, span.text)) {
                note_opaque(index, file_path, content, j);
            }
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
    if (!js_assignment) {
        return std::string::npos;
    }
    size_t expr_end = end_of_js_expression(content, j);
    note_expression(index, file_path, content, j,
                    scan_class_expression(index, file_path, content, j, expr_end));
    return expr_end > j ? expr_end : j + 1;
}

struct ArgSpan {
    size_t begin = 0;
    size_t end = 0;
};

std::vector<ArgSpan> split_args(const std::string& content, size_t open, size_t close) {
    std::vector<ArgSpan> args;
    size_t start = open + 1;
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    for (size_t i = open + 1; i < close && i < content.size();) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            i = comment < close ? comment : close;
            continue;
        }
        if (content[i] == '"' || content[i] == '\'') {
            i = read_quoted(content, i).end;
            continue;
        }
        if (content[i] == '`') {
            i = read_template(content, i).end;
            continue;
        }
        if (content[i] == '(') {
            ++paren;
        } else if (content[i] == ')' && paren > 0) {
            --paren;
        } else if (content[i] == '{') {
            ++brace;
        } else if (content[i] == '}' && brace > 0) {
            --brace;
        } else if (content[i] == '[') {
            ++bracket;
        } else if (content[i] == ']' && bracket > 0) {
            --bracket;
        } else if (content[i] == ',' && paren == 0 && brace == 0 && bracket == 0) {
            args.push_back({start, i});
            start = i + 1;
        }
        ++i;
    }
    args.push_back({start, close});
    return args;
}

bool quoted_class_attribute_name(const std::string& content, size_t begin, size_t end) {
    size_t i = skip_gap(content, begin, end);
    if (i >= end || (content[i] != '"' && content[i] != '\'')) {
        return false;
    }
    TextSpan span = read_quoted(content, i);
    if (skip_gap(content, span.end, end) != end) {
        return false;
    }
    std::string name = utils::to_lower(span.text);
    return name == "class" || name == "classname" || name == "cssclass";
}

size_t try_host_binding(UsageIndex& index,
                        const std::string& file_path,
                        const std::string& content,
                        size_t i) {
    size_t end = 0;
    if (!match_keyword(content, i, "HostBinding", end)) {
        return std::string::npos;
    }
    size_t open = skip_ws(content, end);
    if (open >= content.size() || content[open] != '(') {
        return std::string::npos;
    }
    size_t close = find_matching(content, open, '(', ')');
    if (close == std::string::npos) {
        return std::string::npos;
    }
    std::vector<ArgSpan> args = split_args(content, open, close);
    if (args.empty()) {
        return std::string::npos;
    }
    size_t name_at = skip_gap(content, args[0].begin, args[0].end);
    if (name_at >= args[0].end || (content[name_at] != '"' && content[name_at] != '\'')) {
        return std::string::npos;
    }
    TextSpan span = read_quoted(content, name_at);
    if (skip_gap(content, span.end, args[0].end) != args[0].end) {
        return std::string::npos;
    }
    std::string lower = utils::to_lower(span.text);
    if (lower.rfind("classname.", 0) == 0 || lower.rfind("cssclass.", 0) == 0 || lower.rfind("class.", 0) == 0) {
        size_t dot = span.text.rfind('.');
        if (dot != std::string::npos) {
            add_token(index, span.text.substr(dot + 1), file_path, content, name_at, "host");
        }
        return close + 1;
    }
    if (lower != "class" && lower != "classname" && lower != "cssclass") {
        return std::string::npos;
    }

    size_t cursor = close + 1;
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    while (cursor < content.size()) {
        size_t comment = skip_source_comment(content, cursor);
        if (comment != cursor) {
            cursor = comment;
            continue;
        }
        if (content[cursor] == '"' || content[cursor] == '\'') {
            cursor = read_quoted(content, cursor).end;
            continue;
        }
        if (content[cursor] == '`') {
            cursor = read_template(content, cursor).end;
            continue;
        }
        if (paren == 0 && brace == 0 && bracket == 0 && (content[cursor] == '{' || content[cursor] == ';')) {
            note_opaque(index, file_path, content, i);
            return cursor;
        }
        if (paren == 0 && brace == 0 && bracket == 0 && content[cursor] == '=') {
            if (cursor + 1 < content.size() && (content[cursor + 1] == '=' || content[cursor + 1] == '>')) {
                ++cursor;
                continue;
            }
            ++cursor;
            size_t expr_end = end_of_js_expression(content, cursor);
            note_expression(index, file_path, content, cursor,
                            scan_class_expression(index, file_path, content, cursor, expr_end));
            return expr_end > cursor ? expr_end : cursor + 1;
        }
        if (content[cursor] == '(') {
            ++paren;
        } else if (content[cursor] == ')' && paren > 0) {
            --paren;
        } else if (content[cursor] == '{') {
            ++brace;
        } else if (content[cursor] == '}' && brace > 0) {
            --brace;
        } else if (content[cursor] == '[') {
            ++bracket;
        } else if (content[cursor] == ']' && bracket > 0) {
            --bracket;
        }
        ++cursor;
    }
    note_opaque(index, file_path, content, i);
    return content.size();
}

size_t try_set_attribute(UsageIndex& index,
                         const std::string& file_path,
                         const std::string& content,
                         size_t i) {
    size_t end = 0;
    bool namespaced = match_keyword(content, i, "setAttributeNS", end);
    if (!namespaced && !match_keyword(content, i, "setAttribute", end)) {
        return std::string::npos;
    }
    size_t open = skip_ws(content, end);
    if (open >= content.size() || content[open] != '(') {
        return std::string::npos;
    }
    size_t close = find_matching(content, open, '(', ')');
    if (close == std::string::npos) {
        return std::string::npos;
    }
    std::vector<ArgSpan> args = split_args(content, open, close);
    size_t name_index = namespaced ? 1 : 0;
    size_t value_index = namespaced ? 2 : 1;
    if (args.size() <= value_index || !quoted_class_attribute_name(content, args[name_index].begin, args[name_index].end)) {
        return std::string::npos;
    }
    note_expression(index, file_path, content, open,
                    scan_class_expression(index, file_path, content, args[value_index].begin, args[value_index].end));
    return close + 1;
}

bool is_class_attribute_name(const std::string& name) {
    std::string lower = utils::to_lower(name);
    return lower == "class" || lower == "classname" || lower == "cssclass";
}

void scan_attribute_object(UsageIndex& index,
                           const std::string& file_path,
                           const std::string& content,
                           size_t begin,
                           size_t end) {
    size_t i = begin;
    if (i < end && content[i] == '{') {
        ++i;
    }
    while (i < end) {
        i = skip_gap(content, i, end);
        if (i >= end || content[i] == '}') {
            break;
        }
        std::string key;
        if (content[i] == '[') {
            ++i;
            i = skip_gap(content, i, end);
        }
        if (i < end && (content[i] == '"' || content[i] == '\'')) {
            TextSpan span = read_quoted(content, i);
            key = span.text;
            i = span.end;
        } else if (i < end && (std::isalpha(static_cast<unsigned char>(content[i])) || content[i] == '_')) {
            size_t start = i++;
            while (i < end && is_ident_char(static_cast<unsigned char>(content[i]))) {
                ++i;
            }
            key = content.substr(start, i - start);
        } else {
            ++i;
            continue;
        }
        i = skip_gap(content, i, end);
        if (i < end && content[i] == ']') {
            ++i;
            i = skip_gap(content, i, end);
        }
        if (i >= end || content[i] != ':') {
            continue;
        }
        ++i;
        size_t value_begin = i;
        int paren = 0;
        int brace = 0;
        int bracket = 0;
        while (i < end) {
            size_t comment = skip_source_comment(content, i);
            if (comment != i) {
                i = comment < end ? comment : end;
                continue;
            }
            if (content[i] == '"' || content[i] == '\'') {
                i = read_quoted(content, i).end;
                continue;
            }
            if (content[i] == '`') {
                i = read_template(content, i).end;
                continue;
            }
            if (paren == 0 && brace == 0 && bracket == 0 && (content[i] == ',' || content[i] == '}')) {
                break;
            }
            if (content[i] == '(') {
                ++paren;
            } else if (content[i] == ')' && paren > 0) {
                --paren;
            } else if (content[i] == '{') {
                ++brace;
            } else if (content[i] == '}' && brace > 0) {
                --brace;
            } else if (content[i] == '[') {
                ++bracket;
            } else if (content[i] == ']' && bracket > 0) {
                --bracket;
            }
            ++i;
        }
        if (is_class_attribute_name(key)) {
            note_expression(index, file_path, content, value_begin,
                            scan_class_expression(index, file_path, content, value_begin, i));
        }
        if (i < end && content[i] == ',') {
            ++i;
        }
    }
}

size_t try_attr_or_prop(UsageIndex& index,
                        const std::string& file_path,
                        const std::string& content,
                        size_t i) {
    if (i == 0 || content[i - 1] != '.') {
        return std::string::npos;
    }
    size_t end = 0;
    if (!match_keyword(content, i, "attr", end) && !match_keyword(content, i, "prop", end)) {
        return std::string::npos;
    }
    size_t open = skip_ws(content, end);
    if (open >= content.size() || content[open] != '(') {
        return std::string::npos;
    }
    size_t close = find_matching(content, open, '(', ')');
    if (close == std::string::npos) {
        return std::string::npos;
    }
    std::vector<ArgSpan> args = split_args(content, open, close);
    if (args.empty()) {
        return std::string::npos;
    }
    size_t first = skip_gap(content, args[0].begin, args[0].end);
    if (first < args[0].end && content[first] == '{') {
        scan_attribute_object(index, file_path, content, first, args[0].end);
        return close + 1;
    }
    if (args.size() < 2 || !quoted_class_attribute_name(content, args[0].begin, args[0].end)) {
        return std::string::npos;
    }
    note_expression(index, file_path, content, open,
                    scan_class_expression(index, file_path, content, args[1].begin, args[1].end));
    return close + 1;
}

size_t end_of_js_expression(const std::string& content, size_t i) {
    int paren = 0;
    int brace = 0;
    int bracket = 0;
    while (i < content.size()) {
        size_t comment = skip_source_comment(content, i);
        if (comment != i) {
            if (paren == 0 && brace == 0 && bracket == 0 && content[i] == '/' && i + 1 < content.size() &&
                content[i + 1] == '/') {
                return i;
            }
            i = comment;
            continue;
        }
        if (content[i] == '"' || content[i] == '\'') {
            i = read_quoted(content, i).end;
            continue;
        }
        if (content[i] == '`') {
            i = read_template(content, i).end;
            continue;
        }
        if (paren == 0 && brace == 0 && bracket == 0 && (content[i] == '\n' || content[i] == ';')) {
            return i;
        }
        if (content[i] == '(') {
            ++paren;
        } else if (content[i] == ')') {
            if (paren == 0) {
                return i;
            }
            --paren;
        } else if (content[i] == '{') {
            ++brace;
        } else if (content[i] == '}') {
            if (brace == 0) {
                return i;
            }
            --brace;
        } else if (content[i] == '[') {
            ++bracket;
        } else if (content[i] == ']') {
            if (bracket == 0) {
                return i;
            }
            --bracket;
        }
        ++i;
    }
    return content.size();
}

size_t try_class_call(UsageIndex& index,
                      const std::string& file_path,
                      const std::string& content,
                      size_t i) {
    size_t end = 0;
    bool dotted = i > 0 && content[i - 1] == '.';
    bool matched = match_keyword(content, i, "clsx", end) ||
                   match_keyword(content, i, "cn", end) ||
                   match_keyword(content, i, "classnames", end) ||
                   (dotted && (match_keyword(content, i, "addClass", end) ||
                               match_keyword(content, i, "removeClass", end) ||
                               match_keyword(content, i, "toggleClass", end) ||
                               match_keyword(content, i, "hasClass", end)));
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
              match_keyword(content, j, "remove", method_end) ||
              match_keyword(content, j, "replace", method_end))) {
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

        size_t host_end = try_host_binding(index, file_path, content, i);
        if (host_end != std::string::npos && host_end > i) {
            i = host_end;
            continue;
        }

        size_t attribute_end = try_class_attribute(index, file_path, content, i);
        if (attribute_end != std::string::npos && attribute_end > i) {
            i = attribute_end;
            continue;
        }

        size_t attribute_call_end = try_set_attribute(index, file_path, content, i);
        if (attribute_call_end != std::string::npos && attribute_call_end > i) {
            i = attribute_call_end;
            continue;
        }

        size_t attr_end = try_attr_or_prop(index, file_path, content, i);
        if (attr_end != std::string::npos && attr_end > i) {
            i = attr_end;
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
