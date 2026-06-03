#include "analyzer.hpp"
#include "features/folding_range.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

static FoldingRangeRequestParams make_params(const std::string& uri) {
    FoldingRangeRequestParams p;
    p.textDocument.uri.raw_uri_ = uri;
    return p;
}

static bool has_fold(const std::vector<FoldingRange>& folds, int start, int end) {
    return std::any_of(folds.begin(), folds.end(), [&](const FoldingRange& r) {
        return r.startLine == start && r.endLine == end;
    });
}

// ── module body ───────────────────────────────────────────────────────────

TEST_CASE("foldingRange: module body folds", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_module.sv";
    analyzer.open(uri, R"(module top(
    input logic a,
    output logic b
);
endmodule
)");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    REQUIRE(!folds.empty());
    // module: line 0 (module top) to line 4 (endmodule)
    CHECK(has_fold(folds, 0, 4));
}

TEST_CASE("foldingRange: single-line module excluded", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_single.sv";
    analyzer.open(uri, "module top; endmodule\n");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    // module is on one line — no module fold
    for (const auto& f : folds)
        CHECK_FALSE((f.startLine == 0 && f.endLine == 0));
}

// ── nested begin/end ──────────────────────────────────────────────────────

TEST_CASE("foldingRange: nested begin/end fold independently", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_begin.sv";
    analyzer.open(uri, R"(module top;
    always_ff @(posedge clk) begin
        if (en) begin
            a <= 1;
        end
    end
endmodule
)");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    // outer begin/end: lines 1-5
    CHECK(has_fold(folds, 1, 5));
    // inner begin/end: lines 2-4
    CHECK(has_fold(folds, 2, 4));
    // module: lines 0-6
    CHECK(has_fold(folds, 0, 6));
}

// ── case statement ────────────────────────────────────────────────────────

TEST_CASE("foldingRange: case statement folds", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_case.sv";
    analyzer.open(uri, R"(module top;
    always_comb begin
        case (sel)
            2'b00: a = 1;
            2'b01: begin
                a = 2;
                b = 3;
            end
            default: a = 0;
        endcase
    end
endmodule
)");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    // case/endcase: lines 2-9
    CHECK(has_fold(folds, 2, 9));
    // multi-line case item (begin/end): lines 4-7
    CHECK(has_fold(folds, 4, 7));
}

// ── generate block ────────────────────────────────────────────────────────

TEST_CASE("foldingRange: generate block folds", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_generate.sv";
    analyzer.open(uri, R"(module top;
    generate
        for (genvar i = 0; i < 4; i++) begin : g_loop
            assign out[i] = in[i];
        end
    endgenerate
endmodule
)");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    // generate/endgenerate: lines 1-5
    CHECK(has_fold(folds, 1, 5));
    // loop generate block: lines 2-4
    CHECK(has_fold(folds, 2, 4));
}

// ── block comment ─────────────────────────────────────────────────────────

TEST_CASE("foldingRange: block comment folds", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_block_comment.sv";
    analyzer.open(uri, R"(/* This is
   a multi-line
   block comment */
module top;
endmodule
)");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    // block comment: lines 0-2, kind "comment"
    bool found = std::any_of(folds.begin(), folds.end(), [](const FoldingRange& r) {
        return r.startLine == 0 && r.endLine == 2 && r.kind == "comment";
    });
    CHECK(found);
}

// ── consecutive line comments ─────────────────────────────────────────────

TEST_CASE("foldingRange: consecutive line comments fold as one", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_line_comments.sv";
    analyzer.open(uri, R"(// Line one
// Line two
// Line three
module top;
endmodule
)");
    auto folds = provide_folding_range(analyzer, make_params(uri));
    // three consecutive line comments → one fold [0,2] kind "comment"
    bool found = std::any_of(folds.begin(), folds.end(), [](const FoldingRange& r) {
        return r.startLine == 0 && r.endLine == 2 && r.kind == "comment";
    });
    CHECK(found);
}

// ── idempotency ───────────────────────────────────────────────────────────

TEST_CASE("foldingRange: idempotent", "[folding]") {
    Analyzer    analyzer;
    std::string uri = "file:///tmp/fold_idem.sv";
    analyzer.open(uri, R"(module top;
    always_ff @(posedge clk) begin
        a <= b;
    end
endmodule
)");
    auto p      = make_params(uri);
    auto first  = provide_folding_range(analyzer, p);
    auto second = provide_folding_range(analyzer, p);

    REQUIRE(first.size() == second.size());
    for (size_t i = 0; i < first.size(); ++i) {
        CHECK(first[i].startLine == second[i].startLine);
        CHECK(first[i].endLine   == second[i].endLine);
        CHECK(first[i].kind      == second[i].kind);
    }
}
