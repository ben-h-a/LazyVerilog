<!-- Generated: 2026-05-28 | Updated: 2026-05-28 -->

# lazyverilog

## Purpose
SystemVerilog LSP server written in C++. Provides language intelligence (formatting, linting, go-to-definition, references, auto-wire, auto-instantiation, etc.) for SystemVerilog/Verilog files via the Language Server Protocol.

## Key Files

| File | Description |
|------|-------------|
| `CMakeLists.txt` | CMake build configuration |
| `lazyverilog.toml` | LSP server config — formatting and linting options |
| `CLAUDE.md` | Project instructions for AI agents |
| `README.md` | Project overview and usage |
| `.gitmodules` | Git submodule configuration |

## Subdirectories

| Directory | Purpose |
|-----------|---------|
| `src/` | C++ source — server core + all LSP feature implementations (see `src/AGENTS.md`) |
| `tests/` | Unit and integration tests (see `tests/AGENTS.md`) |
| `docs/` | Documentation for formatter options and diagnostics (see `docs/AGENTS.md`) |
| `tools/` | Dev/benchmark utilities (see `tools/AGENTS.md`) |
| `lua/` | Neovim plugin Lua integration (see `lua/AGENTS.md`) |

## For AI Agents

### Build
```bash
cmake -B build
cmake --build build -j$(nproc)
```

### Testing Requirements
```bash
ctest --test-dir build                          # all tests
./build/lazyverilog-tests "[tag]"               # single feature
./build/lazyverilog-tests "test name here"      # single named test
```

### Working In This Directory
- Core formatting logic: `src/features/formatter.cpp` → `format_source()`
- Config options documented: `docs/formatter/options.md`
- Each LSP feature lives in its own `src/features/*.cpp` file
- Tests mirror feature files: `tests/test_formatter.cpp` tests `src/features/formatter.cpp`

### Common Patterns
- Token-based, idempotent formatting via sequential passes
- Config loaded from `lazyverilog.toml`, walked up from the opened file's directory
- JSON-RPC over stdin/stdout

## Dependencies

### External
- slang (SystemVerilog parser) — via git submodule
- nlohmann/json — JSON-RPC message handling

<!-- MANUAL: -->
