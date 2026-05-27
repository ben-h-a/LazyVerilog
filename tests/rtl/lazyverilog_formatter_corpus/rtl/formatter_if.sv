interface formatter_if #(parameter int Width = `FORMATTER_WIDTH) (input logic clk);
    logic [Width-1:0] req_data;
    logic req_valid;
    logic req_ready;

    modport host(input clk, output req_data, output req_valid, input req_ready), device(input clk, input req_data, input req_valid, output req_ready);
endinterface
