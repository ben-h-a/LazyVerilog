`ifndef FORMATTER_MACROS_SVH
`define FORMATTER_MACROS_SVH

`define FORMATTER_WIDTH 8

`define FORMATTER_BUILD_PACKET(name, width) \
  logic [width-1:0] name``_data; \
    logic           name``_valid; \
      assign name``_valid = |name``_data

`endif // FORMATTER_MACROS_SVH
