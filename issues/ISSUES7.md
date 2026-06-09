# Issue #7 — Known Gaps in Index-Based Generic Go-to-Definition

## Background

`definition_of_state()` uses `find_generic_definition_from_index()` for the extra-file
generic fallback (symbols not caught by Instance/NamedPort/NamedParameter/ClassMember kinds).
This replaces the previous per-file AST walk with index lookups and is faster and covers
closed project files. However two gaps remain.

---

## Gap 1: False-positive risk for same-name typedefs/classes/modules in different scopes

### Affected lookups
`typedef_by_name`, `class_by_name`, `module_by_name` in `find_generic_definition_from_index()`
(`src/analyzer.cpp`).

### Problem
These are single-entry hashmaps keyed by name. When two project files define the same name
in different scopes (e.g. `word_t` globally and `word_t` inside `cpu_pkg`), only one entry
survives in the map. The `pkg_visible()` visibility check runs only on that one entry.

**Scenario:**
```
// file_a.sv  (global scope)
typedef logic [31:0] word_t;

// pkg.sv  (package scope)
package cpu_pkg;
  typedef logic [63:0] word_t;
endpackage
```

If `pkg.sv`'s `word_t` is in the map and the cursor has no `import cpu_pkg`, `pkg_visible`
correctly rejects it — but the global `word_t` is never checked. Result: go-to-def silently
fails instead of jumping to `file_a.sv`.

Conversely, if the global entry is in the map and the cursor imports `cpu_pkg`, it returns
the wrong (global) definition.

### Mitigation
In practice RTL designs rarely have same-name typedefs in different scopes across separate
compilation units. Low occurrence risk but non-zero.

### Fix direction
Change `typedef_by_name` (and potentially `class_by_name`, `module_by_name`) to
`unordered_multimap` or `unordered_map<string, vector<size_t>>` to store all entries per
name, then apply `pkg_visible` to each candidate.

---

## Gap 2: Functions/tasks in closed project files not found

### Affected path
Generic go-to-def for free functions/tasks defined in a closed `.f` filelist file.

### Problem
`syntax_index.cpp` does not add `ValueEntry` records for package-level functions/tasks
(line ~617: only pushes to `package_symbols` string list). Module-level functions/tasks
are also not indexed. Therefore `find_generic_definition_from_index()` cannot find them.

The AST fallback in the extra-file loop only runs for open files. Closed files: no AST,
no `ValueEntry` → symbol not found.

This was pre-existing behaviour (closed files were skipped entirely before the index-based
refactor). Not a regression but a remaining gap.

### Fix direction
In `syntax_index.cpp`, add `ValueEntry` records for `FunctionDeclarationSyntax` members:
- Package-level functions: in the package member loop (~line 617)
- Module-level functions: in the module member loop (~line 373)

Use `fn->prototype->name->as_if<IdentifierNameSyntax>()->identifier` for the name token
and position (same pattern as `GenericDefinitionVisitor`). Set `kind` to `"function"` or
`"task"` based on `fn->prototype->keyword`.
