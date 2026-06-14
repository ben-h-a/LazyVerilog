import * as vscode from "vscode";
import {
  LanguageClient,
  WorkspaceEdit as LspWorkspaceEdit,
} from "vscode-languageclient/node";
import { randomBytes } from "crypto";

// ---------------------------------------------------------------------------
// Server response types
// ---------------------------------------------------------------------------

interface Port {
  name: string;
  type?: string;
  direction?: string;
  signal?: string;
  signal_type?: string;
}

interface Connection {
  inst1_port: string;
  inst2_port: string;
  signal: string;
  signal_type?: string;
}

interface InterfaceData {
  inst1?: { name: string; ports?: Port[] };
  inst2?: { name: string; ports?: Port[] };
  connections?: Connection[];
  error?: string;
}

interface InterfaceRow {
  inst1_port: string;
  inst1_type: string;
  inst1_dir: string;
  signal: string;
  sig_type: string;
  inst2_port: string;
  inst2_type: string;
  inst2_dir: string;
  connected: boolean;
  warn_dir: boolean;
}

interface SingleRow {
  port_type?: string;
  port_name?: string;
  port_dir?: string;
  signal_type?: string;
  signal?: string;
  other_inst?: string;
  other_type?: string;
  other_port?: string;
  other_dir?: string;
}

interface SingleInterfaceData {
  inst?: { name: string };
  rows?: SingleRow[];
  error?: string;
}

// ---------------------------------------------------------------------------
// Build row data (mirrors Lua _interface_show)
// ---------------------------------------------------------------------------

function buildRows(data: InterfaceData): InterfaceRow[] {
  const ports1 = data.inst1?.ports ?? [];
  const ports2 = data.inst2?.ports ?? [];
  const conns = data.connections ?? [];

  const port1_dir: Record<string, string> = {};
  const port1_type: Record<string, string> = {};
  const port2_dir: Record<string, string> = {};
  const port2_type: Record<string, string> = {};
  for (const p of ports1) {
    port1_dir[p.name] = p.direction ?? "";
    port1_type[p.name] = p.type ?? "";
  }
  for (const p of ports2) {
    port2_dir[p.name] = p.direction ?? "";
    port2_type[p.name] = p.type ?? "";
  }

  const rows: InterfaceRow[] = [];
  const covered2 = new Set<string>();

  for (const conn of conns) {
    const d1 = port1_dir[conn.inst1_port] ?? "";
    const d2 = port2_dir[conn.inst2_port] ?? "";
    const connected = conn.signal !== "";
    rows.push({
      inst1_port: conn.inst1_port,
      inst1_type: port1_type[conn.inst1_port] ?? "",
      inst1_dir: d1,
      signal: conn.signal,
      sig_type: conn.signal_type ?? "",
      inst2_port: conn.inst2_port,
      inst2_type: port2_type[conn.inst2_port] ?? "",
      inst2_dir: d2,
      connected,
      warn_dir: connected && d1 !== "" && d2 !== "" && d1 === d2 && d1 !== "inout",
    });
    if (conn.inst2_port !== "") covered2.add(conn.inst2_port);
  }

  for (const p of ports2) {
    if (!covered2.has(p.name)) {
      const d2 = port2_dir[p.name] ?? "";
      const sig = p.signal ?? "";
      rows.push({
        inst1_port: "", inst1_type: "", inst1_dir: "",
        signal: sig, sig_type: p.signal_type ?? "",
        inst2_port: p.name, inst2_type: p.type ?? "", inst2_dir: d2,
        connected: sig !== "",
        warn_dir: false,
      });
    }
  }
  return rows;
}

// ---------------------------------------------------------------------------
// HTML helpers
// ---------------------------------------------------------------------------

function esc(s: string): string {
  return s
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");
}

function dirArrow(from: string, to: string, hasData: boolean): string {
  if (!hasData) return "|";
  if (from === "output" || to === "input") return "→";
  if (from === "input" || to === "output") return "←";
  if (from === "inout") return "↔";
  return "|";
}

const COMMON_STYLE = `
body{font-family:var(--vscode-editor-font-family,monospace);font-size:var(--vscode-editor-font-size,13px);color:var(--vscode-editor-foreground);background:var(--vscode-editor-background);padding:8px}
table{border-collapse:collapse;width:100%}
th,td{padding:2px 8px;text-align:left;white-space:nowrap}
th{border-bottom:1px solid var(--vscode-panel-border)}
tr.dim{opacity:.45}
tr.warn{color:var(--vscode-editorWarning-foreground,#cca700)}
tr.selected{background:var(--vscode-list-activeSelectionBackground);color:var(--vscode-list-activeSelectionForeground)}
td.sep{color:var(--vscode-descriptionForeground);text-align:center;padding:2px 4px}
td.num{color:var(--vscode-descriptionForeground);text-align:right;user-select:none;min-width:2ch}
.toolbar{margin:8px 0;display:flex;gap:8px}
button{padding:4px 12px;background:var(--vscode-button-background);color:var(--vscode-button-foreground);border:none;cursor:pointer}
button:hover{background:var(--vscode-button-hoverBackground)}
button:disabled{opacity:.4;cursor:default}
`.trim();

function renderTwoInstanceHtml(
  inst1_name: string,
  inst2_name: string,
  rows: InterfaceRow[],
  nonce: string,
): string {
  const rowHtml = rows
    .map((r, i) => {
      const cls = r.warn_dir ? "warn" : !r.connected ? "dim" : "";
      const sep12 = dirArrow(r.inst1_dir, "signal", r.inst1_port !== "");
      const sep23 = dirArrow("signal", r.inst2_dir, r.inst2_port !== "");
      return `<tr class="${cls}" data-index="${i}">
      <td class="num">${i + 1}</td>
      <td>${esc(r.inst1_type)}</td><td>${esc(r.inst1_port)}</td>
      <td class="sep">${sep12}</td>
      <td>${esc(r.sig_type)}</td><td>${esc(r.signal)}</td>
      <td class="sep">${sep23}</td>
      <td>${esc(r.inst2_type)}</td><td>${esc(r.inst2_port)}</td>
    </tr>`;
    })
    .join("\n");

  return `<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'nonce-${nonce}'; script-src 'nonce-${nonce}';">
<style nonce="${nonce}">${COMMON_STYLE}</style></head><body>
<table><thead>
<tr><th></th><th colspan="2">${esc(inst1_name)}</th><th></th><th colspan="2">signal</th><th></th><th colspan="2">${esc(inst2_name)}</th></tr>
<tr><th></th><th>type</th><th>name</th><th></th><th>type</th><th>name</th><th></th><th>type</th><th>name</th></tr>
</thead><tbody>${rowHtml}</tbody></table>
<div class="toolbar">
<button id="conn" disabled>Connect</button>
<button id="disc" disabled>Disconnect</button>
</div>
<script nonce="${nonce}">
const vscode=acquireVsCodeApi();
let sel=new Set();
document.querySelectorAll('tbody tr').forEach(tr=>{
  tr.addEventListener('click',()=>{
    const i=parseInt(tr.dataset.index);
    if(sel.has(i)){sel.delete(i);tr.classList.remove('selected');}
    else{sel.add(i);tr.classList.add('selected');}
    upd();
  });
});
function upd(){
  const a=[...sel];
  document.getElementById('conn').disabled=a.length!==2;
  document.getElementById('disc').disabled=a.length!==1;
}
document.getElementById('conn').addEventListener('click',()=>{
  const[i1,i2]=[...sel].sort((a,b)=>a-b);
  vscode.postMessage({type:'connect',row1:i1,row2:i2});
});
document.getElementById('disc').addEventListener('click',()=>{
  const[i]=[...sel];
  vscode.postMessage({type:'disconnect',row:i});
});
</script></body></html>`;
}

function renderSingleInstanceHtml(
  inst_name: string,
  rows: SingleRow[],
  nonce: string,
): string {
  function sep12(port_dir: string, hasSig: boolean): string {
    if (!hasSig) return "|";
    if (port_dir === "output") return "→";
    if (port_dir === "input") return "←";
    if (port_dir === "inout") return "↔";
    return "|";
  }
  function sep23(other_dir: string, hasOther: boolean): string {
    if (!hasOther) return "|";
    if (other_dir === "input") return "→";
    if (other_dir === "output") return "←";
    if (other_dir === "inout") return "↔";
    return "|";
  }

  const rowHtml = rows
    .map((r, i) => {
      const hasSig = (r.signal ?? "") !== "";
      const hasOther = (r.other_port ?? "") !== "";
      return `<tr class="${hasSig ? "" : "dim"}">
      <td class="num">${i + 1}</td>
      <td>${esc(r.port_type ?? "")}</td><td>${esc(r.port_name ?? "")}</td>
      <td class="sep">${sep12(r.port_dir ?? "", hasSig)}</td>
      <td>${esc(r.signal_type ?? "")}</td><td>${esc(r.signal ?? "")}</td>
      <td class="sep">${sep23(r.other_dir ?? "", hasOther)}</td>
      <td>${esc(r.other_inst ?? "")}</td><td>${esc(r.other_type ?? "")}</td><td>${esc(r.other_port ?? "")}</td>
    </tr>`;
    })
    .join("\n");

  return `<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'nonce-${nonce}';">
<style nonce="${nonce}">${COMMON_STYLE}</style></head><body>
<table><thead>
<tr><th></th><th colspan="2">${esc(inst_name)}</th><th></th><th colspan="2">wire</th><th></th><th colspan="3">connections</th></tr>
<tr><th></th><th>type</th><th>name</th><th></th><th>type</th><th>name</th><th></th><th>inst</th><th>type</th><th>port</th></tr>
</thead><tbody>${rowHtml}</tbody></table>
</body></html>`;
}

// ---------------------------------------------------------------------------
// Panel class — single reused webview panel
// ---------------------------------------------------------------------------

class InterfacePanel {
  private static _instance: InterfacePanel | undefined;

  private readonly _panel: vscode.WebviewPanel;
  private _rows: InterfaceRow[] = [];
  private _uri: string = "";
  private _inst1: string = "";
  private _inst2: string = "";

  private constructor(
    context: vscode.ExtensionContext,
    private readonly client: LanguageClient,
  ) {
    this._panel = vscode.window.createWebviewPanel(
      "lazyverilog.interface",
      "Interface",
      vscode.ViewColumn.Beside,
      { enableScripts: true, retainContextWhenHidden: true },
    );
    this._panel.onDidDispose(() => {
      InterfacePanel._instance = undefined;
    }, null, context.subscriptions);
    this._panel.webview.onDidReceiveMessage(
      (msg) => void this._handleMessage(msg),
      undefined,
      context.subscriptions,
    );
  }

  static getOrCreate(
    context: vscode.ExtensionContext,
    client: LanguageClient,
  ): InterfacePanel {
    if (!InterfacePanel._instance) {
      InterfacePanel._instance = new InterfacePanel(context, client);
    }
    return InterfacePanel._instance;
  }

  async loadTwoInstance(uri: string, inst1: string, inst2: string): Promise<void> {
    this._uri = uri;
    this._inst1 = inst1;
    this._inst2 = inst2;

    let data: InterfaceData | null;
    try {
      data = (await this.client.sendRequest("workspace/executeCommand", {
        command: "lazyverilog.interface",
        arguments: [uri, inst1, inst2],
      })) as InterfaceData | null;
    } catch (e) {
      void vscode.window.showErrorMessage(
        `[LazyVerilog] Interface: ${(e as Error).message}`,
      );
      return;
    }
    if (!data) {
      void vscode.window.showWarningMessage("[LazyVerilog] Interface: no data returned");
      return;
    }
    if (data.error) {
      void vscode.window.showErrorMessage(`[LazyVerilog] ${data.error}`);
      return;
    }

    this._rows = buildRows(data);
    const nonce = randomBytes(16).toString("hex");
    this._panel.title = `Interface: ${inst1} ↔ ${inst2}`;
    this._panel.webview.html = renderTwoInstanceHtml(inst1, inst2, this._rows, nonce);
    this._panel.reveal();
  }

  async loadSingleInstance(uri: string, inst: string): Promise<void> {
    this._rows = [];

    let data: SingleInterfaceData | null;
    try {
      data = (await this.client.sendRequest("workspace/executeCommand", {
        command: "lazyverilog.singleInterface",
        arguments: [uri, inst],
      })) as SingleInterfaceData | null;
    } catch (e) {
      void vscode.window.showErrorMessage(
        `[LazyVerilog] Interface: ${(e as Error).message}`,
      );
      return;
    }
    if (!data) {
      void vscode.window.showWarningMessage("[LazyVerilog] Interface: no data returned");
      return;
    }
    if (data.error) {
      void vscode.window.showErrorMessage(`[LazyVerilog] ${data.error}`);
      return;
    }

    const nonce = randomBytes(16).toString("hex");
    this._panel.title = `Interface: ${data.inst?.name ?? inst}`;
    this._panel.webview.html = renderSingleInstanceHtml(
      data.inst?.name ?? inst,
      data.rows ?? [],
      nonce,
    );
    this._panel.reveal();
  }

  private async _handleMessage(msg: {
    type: string;
    row?: number;
    row1?: number;
    row2?: number;
  }): Promise<void> {
    if (msg.type === "connect") {
      const rd1 = this._rows[msg.row1 ?? -1];
      const rd2 = this._rows[msg.row2 ?? -1];
      if (!rd1 || !rd2) return;
      if (!rd1.inst1_port) {
        void vscode.window.showErrorMessage(
          `[LazyVerilog] row ${(msg.row1 ?? 0) + 1} has no inst1 port`,
        );
        return;
      }
      if (!rd2.inst2_port) {
        void vscode.window.showErrorMessage(
          `[LazyVerilog] row ${(msg.row2 ?? 0) + 1} has no inst2 port`,
        );
        return;
      }
      if (
        rd1.inst1_dir !== "" &&
        rd2.inst2_dir !== "" &&
        rd1.inst1_dir === rd2.inst2_dir &&
        rd1.inst1_dir !== "inout"
      ) {
        void vscode.window.showErrorMessage(
          `[LazyVerilog] Cannot connect: both ports are '${rd1.inst1_dir}'`,
        );
        return;
      }
      const wire_name = await vscode.window.showInputBox({ prompt: "Wire name" });
      if (!wire_name) return;
      const wire_type =
        rd1.inst1_dir === "output" && rd1.inst1_type
          ? rd1.inst1_type
          : rd2.inst2_dir === "output" && rd2.inst2_type
            ? rd2.inst2_type
            : rd1.inst1_type || rd2.inst2_type || "logic";
      try {
        const edit = (await this.client.sendRequest("workspace/executeCommand", {
          command: "lazyverilog.interfaceConnect",
          arguments: [
            this._uri, this._inst1, this._inst2,
            rd1.inst1_port, rd2.inst2_port, wire_name, wire_type,
          ],
        })) as LspWorkspaceEdit | null;
        if (edit) {
          const vsEdit = await this.client.protocol2CodeConverter.asWorkspaceEdit(edit);
          await vscode.workspace.applyEdit(vsEdit);
        }
      } catch (e) {
        void vscode.window.showErrorMessage(
          `[LazyVerilog] InterfaceConnect: ${(e as Error).message}`,
        );
        return;
      }
      await this.loadTwoInstance(this._uri, this._inst1, this._inst2);
    } else if (msg.type === "disconnect") {
      const rd = this._rows[msg.row ?? -1];
      if (!rd) return;
      if (!rd.signal) {
        void vscode.window.showWarningMessage(
          `[LazyVerilog] row ${(msg.row ?? 0) + 1} has no connection`,
        );
        return;
      }
      try {
        const edit = (await this.client.sendRequest("workspace/executeCommand", {
          command: "lazyverilog.interfaceDisconnect",
          arguments: [
            this._uri, this._inst1, this._inst2,
            rd.inst1_port, rd.inst2_port, rd.signal,
          ],
        })) as LspWorkspaceEdit | null;
        if (edit) {
          const vsEdit = await this.client.protocol2CodeConverter.asWorkspaceEdit(edit);
          await vscode.workspace.applyEdit(vsEdit);
        }
      } catch (e) {
        void vscode.window.showErrorMessage(
          `[LazyVerilog] InterfaceDisconnect: ${(e as Error).message}`,
        );
        return;
      }
      await this.loadTwoInstance(this._uri, this._inst1, this._inst2);
    }
  }
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

export function registerInterface(
  context: vscode.ExtensionContext,
  client: LanguageClient,
): void {
  context.subscriptions.push(
    vscode.commands.registerCommand("lazyverilog.interface", async () => {
      const editor = vscode.window.activeTextEditor;
      if (!editor) {
        void vscode.window.showErrorMessage("[LazyVerilog] Interface: no active file");
        return;
      }
      const uri = editor.document.uri.toString();
      const inst1 = await vscode.window.showInputBox({
        prompt: "Instance 1 name (or only instance — leave inst2 empty for single view)",
        placeHolder: "e.g. u_cpu",
      });
      if (!inst1) return;
      const inst2 = await vscode.window.showInputBox({
        prompt: "Instance 2 name (empty = single view)",
        placeHolder: "e.g. u_bus",
      });
      const panel = InterfacePanel.getOrCreate(context, client);
      if (inst2) {
        await panel.loadTwoInstance(uri, inst1, inst2);
      } else {
        await panel.loadSingleInstance(uri, inst1);
      }
    }),
  );
}
