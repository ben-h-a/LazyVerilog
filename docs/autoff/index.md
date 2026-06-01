# AutoFF

**Commands:** `lazyverilog.autoffPreview`, `lazyverilog.autoffApply`, `lazyverilog.autoffAllPreview`, `lazyverilog.autoffAllApply`

Inserts missing assignments into an existing `always_ff` block. AutoFF looks for a two-signal declaration on the cursor line — one name matching `lint.naming.register_pattern` (the register destination) and one not (the combinational source) — then inserts `<= '0` in the reset branch and `<= src` in the capture branch for any signal not already assigned.

```systemverilog
// Two-signal declaration: r_count matches register_pattern, w_count does not
logic [7:0] r_count, w_count;

// Existing always_ff in the same file (must already be present):
always_ff @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        // AutoFF inserts: r_count <= '0;
    end else begin
        // AutoFF inserts: r_count <= w_count;
    end
end
```

`autoffApply` / `autoffAllApply` write the edits. `autoffPreview` / `autoffAllPreview` show what would be inserted without applying.

`autoffAllApply` scans the whole file for qualifying two-signal declarations and fills all of them at once.

**Requirements:**
- An `always_ff` block with an `if/else begin` structure must already exist in the file.
- The cursor line must contain a declaration with exactly two signals, one matching the register pattern.

Controlled by the register naming pattern:

```toml
[autoff]
register_pattern = "^r_"
```

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `register_pattern` | string | `"^r_"` | Regex — the signal whose name matches is treated as the register (destination); the other is the source |
