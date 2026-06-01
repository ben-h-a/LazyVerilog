# AutoArg

**Code action** and **on-save**

Generates the non-ANSI port list for a module header by reading the port declarations from the module body.

```systemverilog
// before
module m_alu(
    i_clk, i_rst_n, i_a, i_b, o_result
);
    input  logic        i_clk;
    input  logic        i_rst_n;
    input  logic [7:0]  i_a;
    input  logic [7:0]  i_b;
    output logic [7:0]  o_result;
```

After running AutoArg, the port list `(...)` is regenerated from the body declarations.

As a **code action**, it applies to the module under the cursor.
With **`autoarg_on_save`**, it runs on every module in the file on save.

AutoArg first regenerates the port list from body declarations, then passes the generated module-header replacement through the main formatter. The final layout therefore follows `[format]` and `[format.module]` settings, including indentation and non-ANSI ports-per-line options.

```toml
[autoarg]
autoarg_on_save = true
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `autoarg_on_save` | bool | `false` | Regenerate port lists for all modules in the file on every save |
