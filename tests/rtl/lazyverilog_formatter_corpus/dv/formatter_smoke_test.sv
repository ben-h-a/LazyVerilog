module formatter_smoke_test;
    logic [7:0] a;
    logic [7:0] b;
    logic [7:0] c;

    initial begin
        `uvm_info("formatter", $sformatf("a=%0d b=%0d c=%0d", a, b, c), UVM_LOW)
        void'($urandom_range(0, 10));
    end
endmodule
