`include "params.svh"
`define WIDTH 32
`define print_bytes(ARR, STARTBYTE, NUMBYTES)                \
    for(int ii=STARTBYTE; ii<STARTBYTE+NUMBYTES; ii++) begin \
        if((ii != 0) && (ii % 16 == 0))                      \
            display("\n");                                   \
        $display("0x%x ", ARR[ii]);                          \
    end
function packet_t foo(
    input packet_t i_a,
    input i_b
);
    return packet_t'(i_a + i_b);
endfunction

typedef struct {
    logic               [7:0]               addr                                ;
    logic                                   valid                               ;
} packet_wo_data_t;

typedef struct {
    logic signed        [7:0]               addr                                ;
    logic               [31:0]              data                                ;
    logic                                   valid                               ;
} packet_ta;

typedef enum logic [1:0] {
    IDLE       = 2'd0        ,
    FETCH                    ,
    EXECUTE    = 2'd2        ,
    ERROR
} state_t;

interface bus_intf #(
    parameter W_IDTH = 8
)(
    input     logic      i_clk
);

logic                                   valid                               ;
logic                                   ready                               ;
logic               [`WIDTH123:0]       addr                                ;
logic               [`WIDTH-1:0]        wdata                               ;
logic               [`WIDTH-1:0]        rdata                               ;
logic                                   write                               ;

// DUT view
modport dut (
    input          clk       ,
    input          validdddddddd,
    input          addr      ,
    input          wdata     ,
    input          write     ,
    output         ready     ,
    output         rdata
);

// Testbench/driver view
modport tb (
    input          clk       ,
    input          ready     ,
    input          rdata     ,
    output         valid     ,
    output         addr      ,
    output         wdata     ,
    output         write
);

endinterface

parameter DEPTH = 8;

module memory_top #(
    parameter int WIDTH = 4,
    parameter int DEPTH = 8
)(
    i_clk, i_rst_n, i_data,
    i_data2, i_data3, i_dd,
    i_dd22222, dd22222, i_d33333,
    i_d44333, i_dd44321, i_d44334,
    VDD, VSS, test,
    VSS
);
input                            i_clk                           ;
input                            i_rst_n                         ;
input     logic      [1:0]       i_data            [7:0]         ; // input
input     var byte               i_data2                         ;
input                            i_data3                         ;
input                            i_dd                            ;
input                            i_dd22222                       ;
input                            dd22222                         ;
input                            i_d33333                        ;
input                            i_d44333                        , i_dd44321                  ;
input                            i_d44334                        ;
output    logic unsigned [0:0]   VDD                             , VSS                        ; // output
output    packet_tttttttttttttt [0:0] test                       , VSS                        ; /* output */ // test

logic               [7:0]               dout                = 8'hFF         ;
logic               [8:0]               douteeeeeee         = 8'hFF         ;
packet_tttttttttttttt [1:0]             test_init           = 8'hFF         ;
packet_t                                test_init2          = 8'hFF         ;
logic               [`WIDTH-1:0]        data                                ;

packet_ttttttttttteeettt [1:0]          dp                                  ;
// test
logic               [2:0]               a                                   , b                                   ;
//dd
/* ehlo */
// a, b, c
//
//
//
//
logic               [7:0]               data_out                            ;
logic                                   tt                                  ;
reg signed          [7:0]               kj                                  ;
// logic                                               c                           ;

// b
/* ehlo */
// logic           [2:0]                   d                           ;

automatic int       [3:0]               b                                   ;

wire                [1:0]               addr                                ;
logic                                   address                             ;
logic                                   test                                , r_test                              ; // test
logic                                   test3                               , r_test2                             ;
/* */
logic                                   r_test4                             ;
logic               [`BB-1:0]           data23                              ;
logic               [`BB-1:0]           data32                              ;
[5:0] data32;

assign d          = a + 2;
assign daaa       = a + 2;
assign very_long_text = a + 2;

memory #(.DEPTH(4), .WIDTH(3).WIDTH(2)) u_mem2 (
    .i_clk    (          ),
    .address  (          ),
    .i_data   (data32    ),
    .o_data   (o_data    )
);

memory u_mem3 (
    .i_clk    (          ),
    .address  (          ),
    .i_data   (data23    ),
    .o_data   (data32    )
);

state_t             state                               , r_state                             ;
always_comb begin
    state      = ERROR;
end

endmodule
