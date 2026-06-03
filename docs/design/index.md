# Design & Filelist

Design-wide features (go-to-definition, find references, inlay hints, workspace symbols, AutoInst) require the full module set to be indexed. Provide a filelist to load the design.

```toml
[design]
vcode = "demo/vcode.f"
define = ["RTL_SIM"]
include_dir = ["vendor/uvm/src"]
```

| Option | Type | Description |
|--------|------|-------------|
| `vcode` | string | Path to filelist (`.f`) relative to the `lazyverilog.toml` file |
| `define` | string[] | Preprocessor defines passed to the parser for all design files |
| `include_dir` | string[] | Include search directories, relative to `lazyverilog.toml`, used to resolve `` `include "..." `` directives |

## Filelist format

One file path per line. Paths are relative to the `.f` file's directory.

```
rtl/m_alu.sv
rtl/m_adder.sv
rtl/m_multiplier.sv
```

Parsing rules:

| Syntax | Effect |
|--------|--------|
| `// ...` | Line comment |
| `# ...` | Line comment |
| `+<option>` | Compiler options, including `+incdir+`, are silently ignored |
| `-<flag>` | Compiler flags — silently ignored |

LazyVerilog does not read include directories from `.f` files.  Simulator
options such as `+incdir+vendor/uvm/src` are still ignored.  Use
`[design].include_dir` in `lazyverilog.toml` instead.

## Include directories

`include_dir` lets a package/source file pull in headers without listing every
header as an extra top-level file.  This is useful for libraries such as UVM:

```toml
[design]
vcode = "demo/vcode.f"
include_dir = ["demo/uvm-core/src"]
```

```text
# demo/vcode.f
./uvm-core/src/uvm_pkg.sv
```

With that setup, `uvm_pkg.sv` is the explicit indexed source file, and slang
resolves lines such as:

```systemverilog
`include "base/uvm_base.svh"
`include "comps/uvm_comps.svh"
```

through the configured include directory.  This avoids parsing each UVM `.svh`
as a separate filelist source while still allowing the package parse to discover
classes, typedefs, methods, and macros for completion.
