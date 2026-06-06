#include "formatter.hpp"
#include "formatter_lexer.hpp"
#include "formatter_renderer.hpp"
#include "formatter_log.hpp"

namespace svfmt {

static bool token_stream_same(const TokenStream& a, const TokenStream& b) {
    // safe_mode2 compares the *lexed* token stream before and after formatting.
    // That means we intentionally ignore whitespace-derived positions and all
    // mutable formatting decisions; the goal is to catch semantic/tokenization
    // changes such as a formatter accidentally deleting or rewriting tokens.
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const auto& x = a[i];
        const auto& y = b[i];
        if (x.lex.kind != y.lex.kind)
            return false;
        if (x.lex.text != y.lex.text)
            return false;
        if (x.lex.comment_kind != y.lex.comment_kind)
            return false;
        if (x.lex.is_directive != y.lex.is_directive)
            return false;
        if (x.lex.is_whitespace_sensitive != y.lex.is_whitespace_sensitive)
            return false;
    }
    return true;
}

inline void verify_safe_mode_unchanged(const std::string& source, const std::string& formatted,
                                       const FormatOptions& opts) {
    if (!opts.safe_mode) return;
    if (!ws_equal(source, formatted))
        throw SafeModeError("Formatter safe-mode: non-whitespace content changed — formatting aborted");
}

inline void verify_safe_mode2_unchanged(const TokenStream& before, const std::string& formatted,
                                        const FormatOptions& opts) {
    if (!opts.safe_mode2) return;
    TokenStream after = TokenCollector(formatted, opts).collect();
    if (!token_stream_same(before, after))
        throw SafeModeError("Formatter safe-mode2: token stream changed — formatting aborted");
}

} // namespace svfmt

std::string format_source(const std::string& source, const FormatOptions& opts) {
    svfmt::write_log(opts, "format_source_input.sv", source);

    svfmt::TokenStream tokens = svfmt::TokenCollector(source, opts).collect();
    const svfmt::TokenStream before_tokens = tokens;
    svfmt::write_log(opts, "00_input.sv", source);
    svfmt::write_log(opts, "01_token_stream_collected.log", tokens);

    // Required pass DAG.  Each pass has exclusive write ownership of one
    // metadata family and may only read lexemes plus upstream metadata.
    svfmt::SyntaxPass syntax; syntax.run(tokens);
    svfmt::MacroPass macro(opts); macro.run(tokens);
    svfmt::WrapPass wrap(opts); wrap.run(tokens);
    svfmt::IndentPass indent(opts); indent.run(tokens);
    svfmt::AlignPass align(opts); align.run(tokens);
    svfmt::CommentPass comment; comment.run(tokens);
    svfmt::SpacingPass spacing(opts); spacing.run(tokens);
    svfmt::BlankLinePass blank(opts); blank.run(tokens);

    svfmt::write_log(opts, "98_token_stream_final.log", tokens);
    std::string out = svfmt::render_tokens(tokens);
    svfmt::write_log(opts, "99_output.sv", out);
    svfmt::verify_safe_mode_unchanged(source, out, opts);
    svfmt::verify_safe_mode2_unchanged(before_tokens, out, opts);
    return out;
}
