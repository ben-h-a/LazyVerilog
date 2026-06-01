#include "analyzer.hpp"
#include "features/connect.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("connect: reports modules, ports, and hierarchical instances", "[connect]") {
    Analyzer analyzer;
    const std::string uri = "file:///tmp/connect_fixture.sv";
    analyzer.open(uri, R"(
module producer(output logic [7:0] data);
endmodule
module consumer(input logic [7:0] data);
endmodule
module top;
    producer u_prod (.data());
    consumer u_cons (.data());
endmodule
)");

    const auto json = connect_info_json(analyzer, uri);
    CHECK(json.find("\"producer\"") != std::string::npos);
    CHECK(json.find("\"consumer\"") != std::string::npos);
    CHECK(json.find("top.u_prod") != std::string::npos);
    CHECK(json.find("top.u_cons") != std::string::npos);
}

TEST_CASE("connect: preview and apply produce wiring edits", "[connect]") {
    Analyzer analyzer;
    const std::string uri = "file:///tmp/connect_apply_fixture.sv";
    analyzer.open(uri, R"(
module producer(output logic [7:0] data);
endmodule
module consumer(input logic [7:0] data);
endmodule
module top;
    producer u_prod (.data());
    consumer u_cons (.data());
endmodule
)");

    const auto preview = connect_apply_preview_json(analyzer, uri, "top.u_prod", "data",
                                                    "top.u_cons", "data", "data_w");
    CHECK(preview.find("connect u_prod.data(data_w)") != std::string::npos);
    CHECK(preview.find("declare logic [7:0] data_w in top") != std::string::npos);

    const auto edit = connect_apply_edit_json(analyzer, uri, "top.u_prod", "data",
                                              "top.u_cons", "data", "data_w");
    CHECK(edit.find(".data(data_w)") != std::string::npos);
    CHECK(edit.find("logic [7:0] data_w;") != std::string::npos);
}

TEST_CASE("interface: returns shared signal rows and connect edits", "[connect][interface]") {
    Analyzer analyzer;
    const std::string uri = "file:///tmp/interface_fixture.sv";
    analyzer.open(uri, R"(
module producer(output logic ready);
endmodule
module consumer(input logic ready);
endmodule
module top;
    logic ready_w;
    producer u_prod (.ready(ready_w));
    consumer u_cons (.ready(ready_w));
endmodule
)");

    const auto iface = interface_json(analyzer, uri, "u_prod", "u_cons");
    CHECK(iface.find("\"inst1_port\":\"ready\"") != std::string::npos);
    CHECK(iface.find("\"inst2_port\":\"ready\"") != std::string::npos);
    CHECK(iface.find("ready_w") != std::string::npos);

    const auto single = single_interface_json(analyzer, uri, "u_prod");
    CHECK(single.find("\"other_inst\":\"u_cons\"") != std::string::npos);

    const auto edit = interface_connect_edit_json(analyzer, uri, "u_prod", "u_cons",
                                                  "ready", "ready", "ready2_w", "logic");
    CHECK(edit.find(".ready(ready2_w)") != std::string::npos);
    CHECK(edit.find("logic ready2_w;") != std::string::npos);
}
