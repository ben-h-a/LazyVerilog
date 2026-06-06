# ISSUES4.md

Generated: 2026-06-06

Scope: manual review of `src/` for likely bugs, stale code, HPC/shared-filesystem lag risks, and docs/source mismatches. No source-code changes were made.

## Findings

### 1. Live listed-file edits update `extra_cache_` but do not republish the merged project index

- **Files:** `src/analyzer.cpp:320-337`, `src/analyzer.cpp:390-397`, `src/analyzer.cpp:2886-2905`
- **Type:** correctness / stale index
- **Why it matters:** `Analyzer::open()` and `Analyzer::change()` rebuild a dynamic shard for an open file that is also listed in the design filelist, then call `update_extra_cache_for_live_state_locked()`. That helper updates `extra_cache_` and invalidates the per-file snapshots, but it does not clear or republish `extra_project_index_cache_` and does not schedule a project publish.
- **Likely symptom:** cross-file features that read `Analyzer::extra_project_index()` can see stale module/port/reference facts after an open listed file is edited. This is especially visible for features that do not also merge `opened_files_index()`, such as stale-AutoInst lint on another file.
- **Suggested direction:** after replacing a listed open-buffer shard, either publish a new merged project index, mark a background project publish request, or make all relevant request paths also layer `opened_files_index()` where appropriate.

### 2. Expensive AST/index work runs while holding `Analyzer::map_mutex_`

- **Files:** `src/analyzer.hpp:121-125`, `src/features/connect.cpp:143-152`, `src/features/workspace_symbols.cpp:71-75`, `src/features/inlay_hints.cpp:30-34`, `src/features/autofunc.cpp:250-255`
- **Type:** performance / latency / global serialization
- **Why it matters:** `Analyzer::for_each_state()` invokes the callback while holding `map_mutex_`. Several callbacks call `get_structural_index()` or walk `state->tree`, which can traverse large ASTs and lazily build cached indexes. On large RTL files, this can block unrelated LSP requests, edits, diagnostics, and background commits behind a single global mutex.
- **HPC/shared-environment impact:** this creates UI lag even when the work is CPU-only; if the AST walk triggers include/source-manager behavior or cache misses, the global stall can be worse on shared filesystems.
- **Suggested direction:** change the pattern to snapshot `shared_ptr<const DocumentState>` values under the mutex, release the mutex, then run AST/index work outside the lock.

### 3. Project-index publish callback is executed under `map_mutex_`

- **Files:** `src/analyzer.cpp:2872-2879`, `src/server.cpp:554`, `src/server.cpp:599-609`
- **Type:** performance / possible lock coupling
- **Why it matters:** `publish_extra_project_index_locked()` merges all cached shards and then calls `project_index_publish_callback_()` before releasing `map_mutex_`. The server callback sends `workspace/inlayHint/refresh`; endpoint send/logging/client backpressure should not happen under the analyzer mutex.
- **Likely symptom:** a slow or blocked client notification path can temporarily block analyzer access for normal requests.
- **Suggested direction:** copy/move the callback or set a publish flag while locked, release `map_mutex_`, then notify the client.

### 4. Background semantic compilation can still produce avoidable shared-machine load

- **Files:** `src/background_compiler.cpp:123-125`, `src/background_compiler.cpp:240-270`, `src/config.cpp:136-143`, `src/analyzer.cpp:2442-2480`
- **Type:** HPC/resource risk
- **Why it matters:** configuration clamps worker count only to a minimum of 1, not a safe maximum, and accepts any `nice_value`. A misconfigured project can spawn many semantic compiler threads. Also, every `didOpen` / `didChange` with background compilation enabled builds a full `CompilationSnapshot` over all configured files before debounce coalescing occurs.
- **HPC/shared-environment impact:** even if stale generations are discarded later, preparing repeated full-design snapshots and running many worker threads can burn CPU and memory on shared login/compute nodes.
- **Suggested direction:** cap or warn on high `background_compilation_threads`, validate/clamp `nice_value` to a conservative range, and consider debouncing before constructing the full snapshot or making `schedule()` accept a lightweight generation trigger.

### 5. `lazyverilog.lint` is advertised but intentionally returns `null`

- **Files:** `src/server.cpp:890-908`, `src/server.cpp:1699-1704`
- **Type:** stale/unfinished command surface
- **Why it matters:** the server advertises `lazyverilog.lint` in `executeCommandProvider`, but the execute-command handler has no branch for it and comments that lint returns `null` for now.
- **Likely symptom:** clients may show or call a command that does nothing, which looks like a broken feature.
- **Suggested direction:** either implement the command, remove it from advertised capabilities, or document it as intentionally reserved/no-op.

### 6. `Analyzer::make_file_state()` appears unused production code

- **Files:** `src/analyzer.cpp:290-303`, `src/analyzer.hpp:250`
- **Type:** stale code
- **Why it matters:** grep found only the declaration and definition; current background indexing calls `make_file_state_with_options()` directly. Keeping an unused parse helper makes future refactors riskier because it may not follow newer project-index behavior.
- **Suggested direction:** either remove it, or keep it only if tests/CLI paths need it and add a comment explaining why it remains.

### 7. Docs omit an advertised Connect command

- **Files:** `src/server.cpp:896-897`, `src/server.cpp:1623-1632`, `docs/connect.md:16-20`
- **Type:** docs/source mismatch
- **Mismatch:** the server advertises and handles `lazyverilog.connectHierarchyChildren`, but `docs/connect.md` lists only `connectInfo`, `connectApplyPreview`, and `connectApply`.
- **Suggested direction:** update the Connect docs to include the lazy hierarchy command and its arguments/return shape.

### 8. Header comments for `set_extra_files()` describe old mtime/stat behavior

- **Files:** `src/analyzer.hpp:133-135`, `src/analyzer.hpp:303-306`, `docs/dev/files.md:51-54`
- **Type:** docs/source mismatch / stale comment
- **Mismatch:** the public comment for `set_extra_files()` says `filelist_path` is used to detect filelist changes with a stat per request, but nearby architecture comments and docs say request paths intentionally do not poll or stat the filelist.
- **Suggested direction:** update the header comment to match current behavior: `filelist_path` is retained for config identity/resolution only, and reloads/watch notifications drive freshness.

### 9. Docs say open listed buffers replace filelist shards, but the merged project snapshot can remain stale

- **Files:** `docs/dev/files.md:63-65`, `src/analyzer.cpp:332-336`, `src/analyzer.cpp:392-396`, `src/analyzer.cpp:2886-2905`
- **Type:** docs/source mismatch connected to Finding 1
- **Mismatch:** docs state that an open listed file's in-memory dynamic shard replaces the disk-backed filelist shard. The per-file `extra_cache_` does get replaced, but `extra_project_index_cache_` may not be republished after that replacement.
- **Suggested direction:** either fix the publish behavior or qualify the docs to explain which request paths see the replacement immediately and which wait for the next project-index publish.
