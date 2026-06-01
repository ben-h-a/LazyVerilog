# Connect

`Connect` is an interactive Neovim command for wiring one module instance output to
another module instance input through their nearest common parent.

## Commands

- `:Connect <source_module> <dest_module>`
  - shows instances of `source_module`
  - shows output ports on the selected source instance
  - shows instances of `dest_module`
  - shows input ports on the selected destination instance
  - asks for a wire name
  - shows a confirmation preview before applying edits

The server-side commands are:

- `lazyverilog.connectInfo`
- `lazyverilog.connectApplyPreview`
- `lazyverilog.connectApply`

## Behavior

Connect uses the current open buffers plus configured design filelist files to
find module declarations and instantiations. The command builds hierarchical
instance paths from the syntax index, then generates local text edits:

- connect or replace `.port(signal)` on the selected source and destination instances
- declare the requested wire in the common parent module when it does not already exist
- for cross-hierarchy paths, add pass-through ports on intermediate modules
- warn in the preview when an existing connection will be overwritten
- warn but continue on source/destination type mismatch, using the source output type

## Limitations

The implementation is syntax-index based. It intentionally avoids a full semantic
elaboration pass, so it is best for named port connections in regular module
instantiations. Complex generate-time hierarchy, interfaces/modports, positional
connections, and macro-generated instance text may need manual edits.
