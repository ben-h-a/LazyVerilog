`include "formatter_macros.svh"

module formatter_top #(parameter int Width = `FORMATTER_WIDTH) (
    input logic clk,
    input logic rst_n,
    output logic [Width-1:0] out_data
);
    typedef enum logic [1:0] { Idle = 0, Busy = 1, Done = 2 } state_e;

    state_e state_q, state_d;
    formatter_if #(.Width(Width)) bus_if(.clk(clk));

`FORMATTER_BUILD_PACKET(req, Width)

    function automatic logic [Width-1:0] add_number(input logic [Width-1:0] a, input logic [Width-1:0] b, input logic [Width-1:0] c);
        add_number = a + b + c;
    endfunction

    always_comb begin
        state_d = state_q;
        out_data = add_number(bus_if.req_data, `FORMATTER_WIDTH, Width'(1));
        add_number(
`ifdef FORMATTER_USE_ALT
            .a(bus_if.req_data),
`endif
            .b(out_data), .c(Width'(2)));
        // verilog-format: off
        if(out_data[0])begin state_d=Done; end
        // verilog-format: on
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state_q <= Idle;
        end else begin
            state_q <= state_d;
        end
    end
endmodule
