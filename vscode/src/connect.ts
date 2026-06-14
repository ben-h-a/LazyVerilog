import * as vscode from "vscode";
import {
  LanguageClient,
  WorkspaceEdit as LspWorkspaceEdit,
} from "vscode-languageclient/node";

// ---------------------------------------------------------------------------
// Server response types
// ---------------------------------------------------------------------------

interface PortInfo {
  name: string;
  direction?: string;
  type?: string;
}

interface ModuleInfo {
  ports?: PortInfo[];
  instances?: { hierarchical_path?: string; inst_name?: string; module_name?: string }[];
}

interface HierarchyNode {
  hierarchical_path?: string;
  inst_name?: string;
  module_name?: string;
  root?: boolean;
}

interface ConnectInfoResult {
  modules?: Record<string, ModuleInfo>;
  roots?: HierarchyNode[];
  error?: string;
}

interface ConnectPreview {
  wire_name?: string;
  wire_type?: string;
  lca_module?: string;
  edits?: { file?: string; line?: number; description?: string; is_warning?: boolean }[];
  warnings?: string[];
  error?: string;
}

// ---------------------------------------------------------------------------
// Path helpers (mirrors Lua _connect_boundary_paths)
// ---------------------------------------------------------------------------

function splitPath(path: string): string[] {
  return path.split(".");
}

function joinPath(parts: string[], count: number): string {
  return parts.slice(0, count).join(".");
}

function lcaLen(pathA: string, pathB: string): [number, string[], string[]] {
  const a = splitPath(pathA);
  const b = splitPath(pathB);
  const n = Math.min(a.length, b.length);
  let i = 0;
  while (i < n && a[i] === b[i]) i++;
  return [i, a, b];
}

// Returns [sourceUp, destDown] boundary paths between two leaf paths.
// Matches Lua _connect_boundary_paths exactly (1-indexed → 0-indexed translated).
function boundaryPaths(sourcePath: string, destPath: string): [string[], string[]] {
  const [lca, src, dst] = lcaLen(sourcePath, destPath);
  const sourceUp: string[] = [];
  for (let i = src.length - 1; i >= lca + 1; i--) {
    sourceUp.push(joinPath(src, i));
  }
  const destDown: string[] = [];
  for (let i = lca + 1; i <= dst.length - 1; i++) {
    destDown.push(joinPath(dst, i));
  }
  return [sourceUp, destDown];
}

// ---------------------------------------------------------------------------
// Instance selection — BFS hierarchy traversal
// ---------------------------------------------------------------------------

async function selectInstance(
  client: LanguageClient,
  uri: string,
  roots: HierarchyNode[],
  targetModule: string,
  pathToModule: Record<string, string>,
): Promise<HierarchyNode | undefined> {
  const queue: HierarchyNode[] = [...roots];
  const seen = new Set<string>();
  const matches: HierarchyNode[] = [];
  let visited = 0;
  const MAX = 10000;

  while (queue.length > 0) {
    if (visited >= MAX) {
      void vscode.window.showWarningMessage(
        `[LazyVerilog] Connect: stopped after ${MAX} nodes — narrow the project/filelist`,
      );
      break;
    }
    const node = queue.shift()!;
    const path = node.hierarchical_path ?? "";
    if (!path || seen.has(path)) continue;
    seen.add(path);
    visited++;
    if (node.module_name) pathToModule[path] = node.module_name;

    if (!node.root && node.module_name === targetModule) {
      matches.push(node);
      continue; // don't expand past target
    }

    let result: { children?: HierarchyNode[]; error?: string } | null;
    try {
      result = (await client.sendRequest("workspace/executeCommand", {
        command: "lazyverilog.connectHierarchyChildren",
        arguments: [uri, path],
      })) as { children?: HierarchyNode[] } | null;
    } catch {
      break;
    }
    for (const child of result?.children ?? []) {
      if (child.hierarchical_path && !seen.has(child.hierarchical_path)) {
        if (child.module_name) pathToModule[child.hierarchical_path] = child.module_name;
        queue.push(child);
      }
    }
  }

  if (matches.length === 0) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: no instances of '${targetModule}' found`,
    );
    return undefined;
  }

  matches.sort((a, b) =>
    (a.hierarchical_path ?? "").localeCompare(b.hierarchical_path ?? ""),
  );
  if (matches.length === 1) return matches[0];

  const items = matches.map((m) => ({
    label: m.inst_name ?? m.hierarchical_path ?? "",
    description: m.hierarchical_path ?? "",
    node: m,
  }));
  const picked = await vscode.window.showQuickPick(items, {
    placeHolder: `Select ${targetModule} instance`,
  });
  return picked?.node;
}

// ---------------------------------------------------------------------------
// Pick from candidates or type a custom name
// ---------------------------------------------------------------------------

const CUSTOM_LABEL = "$(edit) Type a new name…";

async function pickOrType(
  prompt: string,
  candidates: string[],
  defaultVal = "",
): Promise<string | undefined> {
  if (candidates.length > 0) {
    const items = [...candidates, CUSTOM_LABEL];
    const picked = await vscode.window.showQuickPick(items, { placeHolder: prompt });
    if (!picked) return undefined;
    if (picked === CUSTOM_LABEL) {
      return vscode.window.showInputBox({ prompt, value: defaultVal });
    }
    return picked;
  }
  return vscode.window.showInputBox({ prompt, value: defaultVal });
}

// ---------------------------------------------------------------------------
// Port candidates for a boundary path
// ---------------------------------------------------------------------------

function portCandidates(
  modules: Record<string, ModuleInfo>,
  pathToModule: Record<string, string>,
  boundaryPath: string,
  direction: string,
): [string[], string | undefined] {
  const moduleName = pathToModule[boundaryPath];
  const mod = moduleName ? modules[moduleName] : undefined;
  if (!mod) return [[], moduleName];
  const names = (mod.ports ?? [])
    .filter((p) => !direction || p.direction === direction)
    .map((p) => p.name)
    .sort();
  return [names, moduleName];
}

// ---------------------------------------------------------------------------
// Main connect wizard
// ---------------------------------------------------------------------------

async function connectFlow(
  client: LanguageClient,
  uri: string,
  module1: string,
  module2: string,
): Promise<void> {
  // Step 1: fetch connect info
  let info: ConnectInfoResult | null;
  try {
    info = (await client.sendRequest("workspace/executeCommand", {
      command: "lazyverilog.connectInfo",
      arguments: [uri, "lazy"],
    })) as ConnectInfoResult | null;
  } catch (e) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: ${(e as Error).message}`,
    );
    return;
  }
  if (!info || info.error) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: ${info?.error ?? "no data"}`,
    );
    return;
  }

  const modules = info.modules ?? {};
  const roots = info.roots ?? [];

  if (!modules[module1]) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: module '${module1}' not found`,
    );
    return;
  }
  if (!modules[module2]) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: module '${module2}' not found`,
    );
    return;
  }

  if (roots.length === 0) {
    void vscode.window.showErrorMessage(
      "[LazyVerilog] Connect: no hierarchy roots found",
    );
    return;
  }

  const pathToModule: Record<string, string> = {};
  for (const [name, mod] of Object.entries(modules)) {
    for (const inst of mod.instances ?? []) {
      if (inst.hierarchical_path) pathToModule[inst.hierarchical_path] = name;
    }
  }

  // Step 2: pick inst1
  const inst1 = await selectInstance(client, uri, roots, module1, pathToModule);
  if (!inst1) return;

  // Step 3: pick output port of module1
  const outPorts1 = (modules[module1].ports ?? [])
    .filter((p) => p.direction === "output")
    .map((p) => p.name)
    .sort();
  const port1Name = await pickOrType(
    `Source output port of ${module1}:`,
    outPorts1,
  );
  if (!port1Name) return;

  // Step 4: pick inst2
  const inst2 = await selectInstance(client, uri, roots, module2, pathToModule);
  if (!inst2) return;

  // Step 5: pick input port of module2
  const inPorts2 = (modules[module2].ports ?? [])
    .filter((p) => p.direction === "input")
    .map((p) => p.name)
    .sort();
  const port2Name = await pickOrType(
    `Destination input port of ${module2}:`,
    inPorts2,
  );
  if (!port2Name) return;

  // Step 6: wire name
  const connectRoute = `${inst1.hierarchical_path}.${port1Name} -> ${inst2.hierarchical_path}.${port2Name}`;
  const wireName = await vscode.window.showInputBox({
    prompt: `Wire name at common root  [${connectRoute}]`,
  });
  if (!wireName) return;

  // Step 7: boundary port prompts
  const [sourceUp, destDown] = boundaryPaths(
    inst1.hierarchical_path ?? "",
    inst2.hierarchical_path ?? "",
  );

  const sourcePorts: string[] = [];
  for (const bPath of sourceUp) {
    const [cands, bMod] = portCandidates(modules, pathToModule, bPath, "output");
    const label = bMod ? `${bPath} [${bMod}]` : bPath;
    const name = await pickOrType(
      `Output port on ${label} to export ${port1Name}:`,
      cands,
      port1Name,
    );
    if (!name) return;
    sourcePorts.push(name);
  }

  // dest boundary prompts are asked source→dest (down), but applied leaf-up
  const destPortsDown: string[] = [];
  for (const bPath of destDown) {
    const [cands, bMod] = portCandidates(modules, pathToModule, bPath, "input");
    const label = bMod ? `${bPath} [${bMod}]` : bPath;
    const name = await pickOrType(
      `Input port on ${label} to import ${port2Name}:`,
      cands,
      port2Name,
    );
    if (!name) return;
    destPortsDown.push(name);
  }
  const destPortsLeafUp = [...destPortsDown].reverse();

  // Step 8: preview
  const applyArgs = [
    uri,
    inst1.hierarchical_path ?? "",
    port1Name,
    inst2.hierarchical_path ?? "",
    port2Name,
    wireName,
    sourcePorts.join("\n"),
    destPortsLeafUp.join("\n"),
  ];

  let preview: ConnectPreview | null;
  try {
    preview = (await client.sendRequest("workspace/executeCommand", {
      command: "lazyverilog.connectApplyPreview",
      arguments: applyArgs,
    })) as ConnectPreview | null;
  } catch (e) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: ${(e as Error).message}`,
    );
    return;
  }
  if (!preview || preview.error) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect: ${preview?.error ?? "no preview"}`,
    );
    return;
  }

  const previewLines: string[] = [
    `Connect: ${preview.wire_type ?? ""} ${preview.wire_name ?? ""}  (wire at ${preview.lca_module ?? ""})`,
    "",
    ...(preview.edits ?? []).map(
      (e) =>
        `${e.is_warning ? "⚠ " : "✓ "}${e.file ?? ""}:${e.line ?? 0}  ${e.description ?? ""}`,
    ),
    ...(preview.warnings ?? []).map((w) => `⚠ ${w}`),
  ];

  const answer = await vscode.window.showInformationMessage(
    previewLines.join("\n"),
    { modal: true },
    "Apply",
  );
  if (answer !== "Apply") return;

  // Step 9: apply
  try {
    const result = (await client.sendRequest("workspace/executeCommand", {
      command: "lazyverilog.connectApply",
      arguments: applyArgs,
    })) as (LspWorkspaceEdit & { error?: string; changes?: unknown }) | null;
    if (result?.error) {
      void vscode.window.showErrorMessage(`[LazyVerilog] Connect: ${result.error}`);
      return;
    }
    if (result) {
      const vsEdit = await client.protocol2CodeConverter.asWorkspaceEdit(result);
      await vscode.workspace.applyEdit(vsEdit);
    }
  } catch (e) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] Connect apply: ${(e as Error).message}`,
    );
  }
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

export function registerConnect(
  context: vscode.ExtensionContext,
  client: LanguageClient,
): void {
  context.subscriptions.push(
    vscode.commands.registerCommand("lazyverilog.connect", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        void vscode.window.showErrorMessage("[LazyVerilog] Connect: no active file");
        return;
      }
      const module1 = await vscode.window.showInputBox({
        prompt: "Source module name",
        placeHolder: "e.g. cpu",
      });
      if (!module1) return;
      const module2 = await vscode.window.showInputBox({
        prompt: "Destination module name",
        placeHolder: "e.g. bus",
      });
      if (!module2) return;
      const uri = editor.document.uri.toString();
      await connectFlow(client, uri, module1, module2);
    }),
  );
}
