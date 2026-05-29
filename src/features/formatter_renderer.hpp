#pragma once

#include "formatter_passes.hpp"
#include <cctype>
#include <stdexcept>

namespace svfmt {

struct RenderDecision {
    bool newline_before{false};
    int indent{0};
    int spaces_before{1};
    int blank_lines{0};
    bool passthrough{false};
    int align_target{-1};
};

// Compose is the only place where semantic formatting metadata becomes concrete
// whitespace intent.  Individual passes never append spaces or newlines.
inline RenderDecision compose(const Tok& tok) {
    RenderDecision out;
    out.newline_before = tok.mutable_.wrap.must_break_before || tok.mutable_.comment.force_own_line;
    out.indent = tok.mutable_.indent.base_indent + tok.mutable_.indent.continuation_indent + tok.mutable_.comment.relative_indent;
    out.spaces_before = tok.mutable_.space.suppress_space ? 0 : tok.mutable_.space.spaces_before;
    out.blank_lines = tok.mutable_.blank.before;
    out.passthrough = is_passthrough(tok);
    out.align_target = tok.mutable_.align.enabled ? tok.mutable_.align.target_column : -1;
    return out;
}

inline void trim_trailing_spaces(std::string& out) {
    while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) out.pop_back();
}

inline std::string render_tokens(const TokenStream& tokens) {
    std::string out;
    int col = 0;
    bool at_line_start = true;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Tok& tok = tokens[i];
        const RenderDecision d = compose(tok);

        // Preprocessor conditionals inside a function-call argument list are
        // not stable formatter structure: when the call is normalized into a
        // canonical argument list, keep the argument tokens and omit the
        // conditional wrapper lines.  This matches the formatter's existing
        // "content-preserving, directive-stripping" behavior for call
        // arguments such as:
        //
        //   f(
        //   `ifdef A
        //     .a(a),
        //   `endif
        //     .b(b)
        //   );
        //
        // rendering as just `.a(a), .b(b)`.
        if (tok.lex->is_directive) {
            bool inside_formatted_function_call = false;
            for (size_t n = i; n > 0; --n) {
                size_t open = n - 1;
                if (!kind_is(tokens[open], TK::OpenParenthesis))
                    continue;
                size_t close = tokens[open].immutable.syntax.matching_token;
                if (close != npos && close > i &&
                    (tokens[open].mutable_.wrap.list_kind == WrapListKind::FunctionBlock ||
                     tokens[open].mutable_.wrap.list_kind == WrapListKind::FunctionHanging)) {
                    inside_formatted_function_call = true;
                    break;
                }
            }
            if (inside_formatted_function_call)
                continue;
        }

        if (i > 0 && d.newline_before && !at_line_start) {
            trim_trailing_spaces(out);
            out.push_back('\n');
            col = 0;
            at_line_start = true;
        }
        if (i > 0 && d.blank_lines > 0) {
            if (!at_line_start) {
                trim_trailing_spaces(out);
                out.push_back('\n');
                col = 0;
                at_line_start = true;
            }
            for (int b = 0; b < d.blank_lines; ++b) out.push_back('\n');
        }

        // Passthrough tokens are emitted verbatim.  This is used for disabled
        // regions and whitespace-sensitive macro bodies.  The renderer still
        // owns the surrounding newline decision.
        if (d.passthrough) {
            std::string_view text(tok.lex->text);
            bool after_format_off_comment =
                i > 0 && tokens[i - 1].lex->is_comment && d.passthrough;
            if (after_format_off_comment && at_line_start && !text.empty() && text.front() == '\n' &&
                (text.size() == 1 || text[1] != '\n'))
                text.remove_prefix(1);
            out.append(text.data(), text.size());
            size_t last_nl = text.rfind('\n');
            if (last_nl == std::string::npos) {
                col += static_cast<int>(text.size());
                at_line_start = false;
            } else {
                col = static_cast<int>(text.size() - last_nl - 1);
                at_line_start = col == 0;
            }
            continue;
        }

        if (at_line_start) {
            out.append(std::max(0, d.indent), ' ');
            col = std::max(0, d.indent);
            at_line_start = false;
        } else {
            int spaces = d.spaces_before;
            if (d.align_target >= 0 && col < d.align_target) spaces = std::max(spaces, d.align_target - col);
            out.append(spaces, ' ');
            col += spaces;
        }

        out += tok.lex->text;
        col += static_cast<int>(tok.lex->text.size());

        if (tok.mutable_.wrap.must_break_after) {
            trim_trailing_spaces(out);
            out.push_back('\n');
            col = 0;
            at_line_start = true;
        }
    }

    trim_trailing_spaces(out);
    while (!out.empty() && out.back() == '\n') out.pop_back();
    out.push_back('\n');
    return out;
}

inline std::string strip_ws(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool at_line_start = true;
    bool skipping_pp_conditional = false;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (at_line_start) {
            size_t j = i;
            while (j < s.size() && (s[j] == ' ' || s[j] == '\t'))
                ++j;
            if (j < s.size() && s[j] == '`') {
                std::string_view rest(s.data() + j, s.size() - j);
                if (rest.rfind("`ifdef", 0) == 0 ||
                    rest.rfind("`ifndef", 0) == 0 ||
                    rest.rfind("`elsif", 0) == 0 ||
                    rest.rfind("`else", 0) == 0 ||
                    rest.rfind("`endif", 0) == 0) {
                    skipping_pp_conditional = true;
                }
            }
            at_line_start = false;
        }
        if (c == '\n') {
            skipping_pp_conditional = false;
            at_line_start = true;
            continue;
        }
        if (!skipping_pp_conditional && !std::isspace(c))
            out.push_back(static_cast<char>(c));
    }
    return out;
}

} // namespace svfmt
