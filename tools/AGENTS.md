<!-- Parent: ../AGENTS.md -->
<!-- Generated: 2026-05-28 | Updated: 2026-05-28 -->

# tools

## Purpose
Developer utilities for benchmarking and debugging the lazyverilog server. Not part of the production build.

## Key Files

| File | Description |
|------|-------------|
| `lsp_proxy.py` | Python LSP proxy for manual testing and debugging of JSON-RPC messages |
| `parse_bench.cpp` | C++ parser benchmarking tool |
| `run_parse_bench_opentitan.sh` | Runs formatter performance sweep against OpenTitan RTL corpus |
| `diff_once_twice` | Idempotency checker — formats a `.sv` file once and twice, then diffs pass-by-pass logs to find the first non-idempotent pass |

## For AI Agents

### Working In This Directory
- `lsp_proxy.py` is useful for tracing raw LSP traffic during development
- Benchmark scripts require the OpenTitan RTL submodule (`tests/rtl/opentitan/`)

<!-- MANUAL: -->
