# Interface View

`Interface` is an interactive Neovim view for inspecting and editing the signal
interface between instances in the current design.

## Commands

- `:Interface <inst1> <inst2>` — show a two-instance interface table.
- `:Interface <inst>` — show a single-instance table with sibling connections.

The server-side commands are:

- `lazyverilog.interface`
- `lazyverilog.singleInterface`
- `lazyverilog.interfaceConnect`
- `lazyverilog.interfaceDisconnect`

## Two-instance view

The two-instance view lists ports from the first instance, shared signal names,
and matching ports from the second instance. From the floating window:

- `C` connects two selected rows using a requested wire name
- `D` disconnects a selected row
- `q` closes the view

Connecting replaces or adds `.port(wire)` on both instances and declares the wire
in the current module if it is not already declared. Disconnecting clears the
selected port connections and removes a standalone declaration for the signal
when found.

## Single-instance view

The single-instance view lists each port, its connected signal, and sibling
instance ports that share the same signal. This is a read-only inspection view.

## Limitations

Interface view currently works from syntax-index information in open buffers and
design filelist files. It expects named port connections and ordinary module
instances. It does not elaborate SystemVerilog `interface` constructs or modports.
