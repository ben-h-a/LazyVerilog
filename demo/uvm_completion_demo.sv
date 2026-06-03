// Small manual fixture for testing UVM completion through the normal library
// indexing path.
//
// Expected setup:
//   demo/vcode.f lists ./uvm-core/src/uvm_pkg.sv explicitly.
//
// LazyVerilog intentionally does not use a hardcoded UVM completion provider
// and does not interpret +incdir+.  UVM symbols below should therefore come
// from the downloaded UVM source files, exactly like symbols from any other
// library package.

import uvm_pkg::*;

class uvm_completion_demo extends uvm_component;
  // Try completion after `uvm_pkg::` to see package-visible UVM symbols.
  // Try completion after `uvm_` to see imported/indexed UVM identifiers and
  // macros discovered from the filelist-indexed UVM package.

  function new(string name = "uvm_completion_demo", uvm_component parent = null);
    super.new(name, parent);
  endfunction

  function void build_phase(uvm_phase phase);
    super.build_phase(phase);
    uvm_pkg::
  endfunction
endclass
