#include "analyzer.hpp"
#include "syntax_index.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <slang/syntax/SyntaxTree.h>

static const std::string kMemory = R"(
module memory #(parameter WIDTH=8, DEPTH=256) (
    input  logic        clk,
    input  logic        we,
    input  logic [7:0]  addr,
    input  logic [7:0]  din,
    output logic [7:0]  dout
);
    logic [WIDTH-1:0] mem [0:DEPTH-1];
    always_ff @(posedge clk) begin
        if (we) mem[addr] <= din;
        dout <= mem[addr];
    end
endmodule

module top (
    input logic clk
);
    memory u_mem (
        .clk(clk), .we(1'b0), .addr(8'h0), .din(8'h0), .dout()
    );
endmodule
)";

TEST_CASE("syntax_index: finds memory module and its ports", "[index]") {
    auto tree = slang::syntax::SyntaxTree::fromText(kMemory);
    REQUIRE(tree != nullptr);

    auto idx = SyntaxIndex::build(*tree, kMemory);

    auto it = std::find_if(idx.modules.begin(), idx.modules.end(),
                           [](const ModuleEntry& m) { return m.name == "memory"; });
    REQUIRE(it != idx.modules.end());
    REQUIRE(idx.module_by_name.contains("memory"));
    CHECK(idx.modules[idx.module_by_name.at("memory")].name == "memory");
    CHECK(it->line > 0);
    CHECK(it->ports.size() >= 5);

    auto port_it = std::find_if(it->ports.begin(), it->ports.end(),
                                [](const PortEntry& p) { return p.name == "clk"; });
    REQUIRE(port_it != it->ports.end());
    REQUIRE(it->port_by_name.contains("clk"));
    CHECK(it->ports[it->port_by_name.at("clk")].name == "clk");
    CHECK(port_it->direction == "input");
}

TEST_CASE("syntax_index: finds top module", "[index]") {
    auto tree = slang::syntax::SyntaxTree::fromText(kMemory);
    auto idx = SyntaxIndex::build(*tree, kMemory);

    auto it = std::find_if(idx.modules.begin(), idx.modules.end(),
                           [](const ModuleEntry& m) { return m.name == "top"; });
    REQUIRE(it != idx.modules.end());
}

TEST_CASE("syntax_index: finds memory instantiation in top", "[index]") {
    auto tree = slang::syntax::SyntaxTree::fromText(kMemory);
    auto idx = SyntaxIndex::build(*tree, kMemory);

    auto it = std::find_if(idx.instances.begin(), idx.instances.end(),
                           [](const InstanceEntry& e) { return e.module_name == "memory"; });
    REQUIRE(it != idx.instances.end());
    CHECK(it->instance_name == "u_mem");
    CHECK(it->parent_module == "top");
}

TEST_CASE("syntax_index: standalone package root preserves package symbols", "[index]") {
    const std::string text =
        "package standalone_pkg;\n"
        "    typedef enum { PKG_IDLE, PKG_DONE } pkg_state_t;\n"
        "    parameter int PKG_WIDTH = 8;\n"
        "endpackage\n";
    auto tree = slang::syntax::SyntaxTree::fromText(text);
    REQUIRE(tree != nullptr);

    auto idx = SyntaxIndex::build(*tree, text);

    CHECK(idx.package_names.contains("standalone_pkg"));
    REQUIRE(idx.package_symbols.contains("standalone_pkg"));
    const auto& symbols = idx.package_symbols.at("standalone_pkg");
    CHECK(std::find(symbols.begin(), symbols.end(), "pkg_state_t") != symbols.end());
    CHECK(std::find(symbols.begin(), symbols.end(), "PKG_IDLE") != symbols.end());
    CHECK(std::find(symbols.begin(), symbols.end(), "PKG_WIDTH") != symbols.end());
}

TEST_CASE("syntax_index: standalone interface root preserves interface identity", "[index]") {
    const std::string text =
        "interface standalone_if;\n"
        "    logic req;\n"
        "    modport master(output req);\n"
        "endinterface\n";
    auto tree = slang::syntax::SyntaxTree::fromText(text);
    REQUIRE(tree != nullptr);

    auto idx = SyntaxIndex::build(*tree, text);

    CHECK(idx.interface_names.contains("standalone_if"));
    REQUIRE(idx.module_by_name.contains("standalone_if"));
    const auto& entry = idx.modules.at(idx.module_by_name.at("standalone_if"));
    CHECK(entry.name == "standalone_if");
    CHECK(entry.modports.size() == 1);
    CHECK(entry.modports[0].name == "master");
}

TEST_CASE("syntax_index: standalone class and typedef roots are indexed", "[index]") {
    {
        const std::string text =
            "class standalone_cfg;\n"
            "    int timeout;\n"
            "endclass\n";
        auto tree = slang::syntax::SyntaxTree::fromText(text);
        REQUIRE(tree != nullptr);

        auto idx = SyntaxIndex::build(*tree, text);
        CHECK(idx.class_by_name.contains("standalone_cfg"));
    }

    {
        const std::string text =
            "typedef struct packed {\n"
            "    logic valid;\n"
            "} standalone_packet_t;\n";
        auto tree = slang::syntax::SyntaxTree::fromText(text);
        REQUIRE(tree != nullptr);

        auto idx = SyntaxIndex::build(*tree, text);
        CHECK(idx.typedef_by_name.contains("standalone_packet_t"));
    }
}
