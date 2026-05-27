# LazyVerilog formatter RTL corpus

Small SystemVerilog fixture project used by `lazyverilog-rtl-format-test`.

The files intentionally mix common formatter stress cases:

- preprocessor define continuation blocks
- function calls with object-like macros and PP conditionals
- format off/on control comments
- enum and modport alignment
- UVM-style macro calls

This corpus is kept small so it can run as part of the default CTest suite.
