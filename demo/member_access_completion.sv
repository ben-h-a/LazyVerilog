// Small completion-focused RTL/UVM-adjacent fixture.
//
// Open this file in Neovim and request completion after `cfg.` below to verify
// that LazyVerilog resolves a variable's declared class type before offering
// member-access completions.

class pkt_cfg;
    int depth;

    function void apply();
    endfunction
endclass

module member_access_completion_demo;
    pkt_cfg cfg;

    initial begin
        cfg.
    end
endmodule
