#include "folding_range.hpp"
#include "document_state.hpp"

#include <algorithm>
#include <slang/syntax/AllSyntax.h>
#include <slang/syntax/SyntaxTree.h>
#include <slang/syntax/SyntaxVisitor.h>
#include <slang/text/SourceLocation.h>
#include <slang/text/SourceManager.h>

using namespace slang;
using namespace slang::syntax;
using namespace slang::parsing;

namespace {

// ── helpers ────────────────────────────────────────────────────────────────

static int token_line(const SourceManager& sm, const Token& tok) {
    if (!tok || !tok.location().valid()) return -1;
    return (int)sm.getLineNumber(tok.location()) - 1;
}

static int first_line(const SourceManager& sm, const SyntaxNode& n) {
    return token_line(sm, n.getFirstToken());
}

static int last_line(const SourceManager& sm, const SyntaxNode& n) {
    return token_line(sm, n.getLastToken());
}

static void emit(std::vector<FoldingRange>& out, int start, int end,
                 const std::string& kind = "region") {
    if (start < 0 || end < 0 || start >= end) return;
    FoldingRange r;
    r.startLine      = start;
    r.endLine        = end;
    r.startCharacter = 0;
    r.endCharacter   = 0;
    r.kind           = kind;
    out.push_back(r);
}

// ── fold visitor ──────────────────────────────────────────────────────────

struct FoldVisitor : public SyntaxVisitor<FoldVisitor> {
    const SourceManager&       sm;
    std::vector<FoldingRange>& out;

    // preprocessor stack: line of the opening ifdef/elsif/else directive
    std::vector<int> pp_stack;

    // celldefine stack: line of the opening `celldefine
    std::vector<int> cell_stack;

    // consecutive line-comment run
    int comment_run_start{-1};
    int comment_run_last{-1};

    FoldVisitor(const SourceManager& sm, std::vector<FoldingRange>& out)
        : sm(sm), out(out) {}

    void flush_comment_run() {
        if (comment_run_start >= 0 && comment_run_last > comment_run_start)
            emit(out, comment_run_start, comment_run_last, "comment");
        comment_run_start = -1;
        comment_run_last  = -1;
    }

    // ── top-level declarations ────────────────────────────────────────────

    // module / macromodule / interface / program / package (all use same type)
    void handle(const ModuleDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const ClassDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const CheckerDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // primitive / UDP
    void handle(const UdpDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const ConfigDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const ClockingDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // function / task (TaskDeclaration maps to FunctionDeclarationSyntax)
    void handle(const FunctionDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // ── statement blocks ──────────────────────────────────────────────────

    // begin/end (SequentialBlockStatement) + fork/join* (ParallelBlockStatement)
    void handle(const BlockStatementSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // if / else if / else — fold the whole chain as one region
    void handle(const ConditionalStatementSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // case / casex / casez / endcase
    void handle(const CaseStatementSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // randcase / endcase
    void handle(const RandCaseStatementSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // individual multi-line case items
    void handle(const StandardCaseItemSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // ── verification constructs ───────────────────────────────────────────

    void handle(const ConstraintDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const CovergroupDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const PropertyDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const SequenceDeclarationSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const SpecifyBlockSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // ── generate constructs ───────────────────────────────────────────────

    void handle(const GenerateRegionSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const LoopGenerateSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const IfGenerateSyntax& node) {
        int node_start = first_line(sm, node);
        int node_end   = last_line(sm, node);
        if (node.elseClause) {
            int else_line = token_line(sm, node.elseClause->elseKeyword);
            // fold the if arm
            emit(out, node_start, else_line - 1);
            // fold the else arm
            emit(out, else_line, node_end);
        } else {
            emit(out, node_start, node_end);
        }
        visitDefault(node);
    }

    void handle(const CaseGenerateSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const GenerateBlockSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // ── port / parameter lists ────────────────────────────────────────────

    void handle(const AnsiPortListSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    void handle(const ParameterPortListSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // instance parameter override #(...)
    void handle(const ParameterValueAssignmentSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // instance connection list (...)
    void handle(const HierarchicalInstanceSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // multi-line argument lists
    void handle(const ArgumentListSyntax& node) {
        emit(out, first_line(sm, node), last_line(sm, node));
        visitDefault(node);
    }

    // ── preprocessor directives ───────────────────────────────────────────

    void handle(const ConditionalBranchDirectiveSyntax& node) {
        flush_comment_run();
        int line = token_line(sm, node.directive);
        if (node.kind == SyntaxKind::IfDefDirective ||
            node.kind == SyntaxKind::IfNDefDirective) {
            pp_stack.push_back(line);
        } else if (node.kind == SyntaxKind::ElsIfDirective) {
            if (!pp_stack.empty()) {
                emit(out, pp_stack.back(), line - 1, "region");
                pp_stack.back() = line;
            }
        }
        visitDefault(node);
    }

    void handle(const UnconditionalBranchDirectiveSyntax& node) {
        flush_comment_run();
        int line = token_line(sm, node.directive);
        if (node.kind == SyntaxKind::ElseDirective) {
            if (!pp_stack.empty()) {
                emit(out, pp_stack.back(), line - 1, "region");
                pp_stack.back() = line;
            }
        } else if (node.kind == SyntaxKind::EndIfDirective) {
            if (!pp_stack.empty()) {
                emit(out, pp_stack.back(), line - 1, "region");
                pp_stack.pop_back();
            }
        }
        visitDefault(node);
    }

    // `celldefine / `endcelldefine
    void handle(const SimpleDirectiveSyntax& node) {
        flush_comment_run();
        int line = token_line(sm, node.directive);
        if (node.kind == SyntaxKind::CellDefineDirective) {
            cell_stack.push_back(line);
        } else if (node.kind == SyntaxKind::EndCellDefineDirective) {
            if (!cell_stack.empty()) {
                emit(out, cell_stack.back(), line, "region");
                cell_stack.pop_back();
            }
        }
        visitDefault(node);
    }

    // ── token trivia (comments) ───────────────────────────────────────────

    void visitToken(Token token) {
        if (!token || !token.location().valid()) return;

        auto    buffer      = token.location().buffer();
        size_t  tok_offset  = token.location().offset();

        // derive trivia start offset: trivia comes immediately before the token
        size_t trivia_total = 0;
        for (const auto& t : token.trivia())
            trivia_total += t.getRawText().size();
        size_t pos = tok_offset >= trivia_total ? tok_offset - trivia_total : 0;

        for (const auto& t : token.trivia()) {
            process_trivia(t, pos, buffer);
            pos += t.getRawText().size();
        }
    }

    void process_trivia(const Trivia& t, size_t offset, BufferID buffer) {
        using TV = TriviaKind;

        if (t.kind == TV::BlockComment) {
            flush_comment_run();
            auto raw       = t.getRawText();
            int  start     = line_at(buffer, offset);
            int  newlines  = (int)std::count(raw.begin(), raw.end(), '\n');
            if (start >= 0 && newlines > 0)
                emit(out, start, start + newlines, "comment");
        } else if (t.kind == TV::LineComment) {
            int line = line_at(buffer, offset);
            if (line < 0) return;
            if (comment_run_start < 0) {
                comment_run_start = line;
                comment_run_last  = line;
            } else if (line == comment_run_last + 1) {
                comment_run_last = line;
            } else {
                flush_comment_run();
                comment_run_start = line;
                comment_run_last  = line;
            }
        } else if (t.kind != TV::Whitespace && t.kind != TV::EndOfLine) {
            // directive or other non-whitespace trivia breaks a comment run
            flush_comment_run();
        }
    }

    int line_at(BufferID buffer, size_t offset) const {
        SourceLocation loc(buffer, offset);
        if (!loc.valid()) return -1;
        return (int)sm.getLineNumber(loc) - 1;
    }
};

// ── import / include group collector ─────────────────────────────────────

static void collect_import_folds(const SourceManager& sm, const SyntaxNode& root,
                                  std::vector<FoldingRange>& out) {
    const auto* unit = root.as_if<CompilationUnitSyntax>();
    if (!unit) return;

    int run_start = -1;
    int run_last  = -1;

    auto flush = [&] {
        if (run_start >= 0 && run_last > run_start)
            emit(out, run_start, run_last, "imports");
        run_start = -1;
        run_last  = -1;
    };

    for (const auto* member : unit->members) {
        if (!member) continue;
        if (member->kind == SyntaxKind::PackageImportDeclaration) {
            int fl = first_line(sm, *member);
            int ll = last_line(sm, *member);
            if (run_start < 0) {
                run_start = fl;
                run_last  = ll;
            } else {
                run_last = ll;
            }
        } else {
            flush();
        }
    }
    flush();
}

} // namespace

// ── public API ────────────────────────────────────────────────────────────

std::vector<FoldingRange> provide_folding_range(const Analyzer& analyzer,
                                                const FoldingRangeRequestParams& params) {
    auto state = analyzer.get_state(params.textDocument.uri.raw_uri_);
    if (!state || !state->tree) return {};

    auto&                    sm = *state->source_manager;
    std::vector<FoldingRange> out;

    FoldVisitor v{sm, out};
    state->tree->root().visit(v);
    v.flush_comment_run(); // flush any trailing comment run

    collect_import_folds(sm, state->tree->root(), out);

    return out;
}
