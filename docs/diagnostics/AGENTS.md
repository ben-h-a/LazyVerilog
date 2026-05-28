<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-05-28 | Updated: 2026-05-28 -->

# docs/diagnostics

## Purpose
Documentation for the lazyverilog diagnostics system, including background compilation behavior.

## Key Files

| File | Description |
|------|-------------|
| `background-compilation.md` | Explains the background compilation pipeline and diagnostic publishing |

## For AI Agents

### Working In This Directory
- Background compiler behavior is implemented in `src/background_compiler.cpp`
- Diagnostics are published via `textDocument/publishDiagnostics` in `src/features/lint.cpp`

<!-- MANUAL: -->
