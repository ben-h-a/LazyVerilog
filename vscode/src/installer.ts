import * as crypto from "crypto";
import * as fs from "fs";
import * as https from "https";
import * as os from "os";
import * as path from "path";
import * as child_process from "child_process";
import * as vscode from "vscode";
import { RELEASE_VERSION } from "./version";
import { RELEASE_CHECKSUMS } from "./checksums";

const RELEASE_BASE_URL =
  "https://github.com/lazyverilog/LazyVerilog/releases/download";

// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------

export function getPlatformKey(): string | undefined {
  const p = process.platform;
  const a = process.arch;

  const osPart =
    p === "linux" ? "linux" : p === "darwin" ? "darwin" : p === "win32" ? "windows" : undefined;
  if (!osPart) return undefined;

  const archPart = a === "x64" ? "x64" : a === "arm64" ? "arm64" : undefined;
  if (!archPart) return undefined;

  return `${osPart}-${archPart}`;
}

// ---------------------------------------------------------------------------
// HTTPS redirect-following download
// ---------------------------------------------------------------------------

function followRedirects(
  url: string,
  maxHops = 5,
  timeout = 30_000,
): Promise<import("http").IncomingMessage> {
  return new Promise((resolve, reject) => {
    if (maxHops <= 0) {
      return reject(new Error("too many redirects"));
    }
    let parsed: URL;
    try {
      parsed = new URL(url);
    } catch (e) {
      return reject(new Error(`invalid URL: ${url}`));
    }
    if (parsed.protocol !== "https:") {
      return reject(new Error(`refusing non-HTTPS redirect: ${url}`));
    }
    const req = https.get(url, (res) => {
      if (
        res.statusCode &&
        res.statusCode >= 300 &&
        res.statusCode < 400 &&
        res.headers.location
      ) {
        res.resume(); // drain before following
        followRedirects(res.headers.location, maxHops - 1, timeout).then(
          resolve,
          reject,
        );
      } else if (res.statusCode === 200) {
        resolve(res);
      } else {
        res.resume();
        reject(new Error(`HTTP ${res.statusCode} for ${url}`));
      }
    });
    req.setTimeout(timeout, () => {
      req.destroy(new Error(`download timed out after ${timeout}ms`));
    });
    req.on("error", reject);
  });
}

// ---------------------------------------------------------------------------
// Binary paths
// ---------------------------------------------------------------------------

function getManagedBinDir(context: vscode.ExtensionContext): string {
  return path.join(context.globalStorageUri.fsPath, "bin");
}

function getManagedBinPath(context: vscode.ExtensionContext): string {
  const suffix = process.platform === "win32" ? ".exe" : "";
  return path.join(getManagedBinDir(context), `lazyverilog-lsp${suffix}`);
}

function isExecutable(p: string): boolean {
  try {
    fs.accessSync(p, fs.constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

function which(name: string): string | undefined {
  try {
    const cmd = process.platform === "win32" ? "where" : "which";
    const result = child_process.execFileSync(cmd, [name], {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "ignore"],
    });
    const p = result.trim().split(/\r?\n/)[0];
    return p && isExecutable(p) ? p : undefined;
  } catch {
    return undefined;
  }
}

function expectedChecksumsForPlatform(platformKey: string): string[] {
  const versionChecksums = RELEASE_CHECKSUMS[RELEASE_VERSION];
  if (!versionChecksums) return [];

  const checksums: string[] = [];
  const expected = versionChecksums[platformKey];
  if (expected) checksums.push(expected.toLowerCase());

  // Linux may have installed the static fallback into the same managed path.
  const staticExpected = versionChecksums[`${platformKey}-static`];
  if (process.platform === "linux" && staticExpected) {
    checksums.push(staticExpected.toLowerCase());
  }

  return checksums;
}

async function managedBinaryMatchesRelease(binPath: string): Promise<boolean> {
  const platformKey = getPlatformKey();
  if (!platformKey) return true;

  const expected = expectedChecksumsForPlatform(platformKey);
  if (expected.length === 0) return true;

  try {
    const actual = (await sha256File(binPath)).toLowerCase();
    return expected.includes(actual);
  } catch {
    return false;
  }
}

export async function resolveServerPath(
  config: vscode.WorkspaceConfiguration,
  context: vscode.ExtensionContext,
): Promise<string | undefined> {
  // 1. User-configured path
  const userPath = config.get<string>("lazyverilog.serverPath", "").trim();
  if (userPath && isExecutable(userPath)) {
    return userPath;
  }

  // 2. PATH
  const onPath =
    which("lazyverilog-lsp") ??
    (process.platform === "win32" ? which("lazyverilog-lsp.exe") : undefined);
  if (onPath) {
    return onPath;
  }

  // 3. Managed binary
  const managed = getManagedBinPath(context);
  if (isExecutable(managed)) {
    if (await managedBinaryMatchesRelease(managed)) {
      return managed;
    }
    fs.rmSync(managed, { force: true });
  }

  return undefined;
}

// ---------------------------------------------------------------------------
// SHA-256 verification
// ---------------------------------------------------------------------------

function sha256File(filePath: string): Promise<string> {
  return new Promise((resolve, reject) => {
    const hash = crypto.createHash("sha256");
    const stream = fs.createReadStream(filePath);
    stream.on("data", (chunk) => hash.update(chunk));
    stream.on("end", () => resolve(hash.digest("hex")));
    stream.on("error", reject);
  });
}

// ---------------------------------------------------------------------------
// Linux compatibility check (ldd)
// ---------------------------------------------------------------------------

function checkLinuxCompat(binPath: string): Promise<boolean> {
  return new Promise((resolve) => {
    child_process.execFile("ldd", [binPath], (err, stdout, stderr) => {
      const output = (stdout ?? "") + (stderr ?? "");
      resolve(!output.includes("not found"));
    });
  });
}

// ---------------------------------------------------------------------------
// Single-flight auto-install
// ---------------------------------------------------------------------------

let installPromise: Promise<string> | null = null;

export function autoInstall(
  context: vscode.ExtensionContext,
): Promise<string> {
  if (installPromise) {
    return installPromise;
  }

  installPromise = _doInstall(context).finally(() => {
    installPromise = null;
  });

  return installPromise;
}

async function _doInstall(
  context: vscode.ExtensionContext,
): Promise<string> {
  const platformKey = getPlatformKey();
  if (!platformKey) {
    const msg = `LazyVerilog: unsupported platform (${process.platform}/${process.arch})`;
    void vscode.window.showErrorMessage(msg);
    throw new Error(msg);
  }

  const versionChecksums = RELEASE_CHECKSUMS[RELEASE_VERSION];
  const expectedChecksum =
    versionChecksums && platformKey in versionChecksums
      ? versionChecksums[platformKey]
      : undefined;

  if (!expectedChecksum) {
    const msg = `LazyVerilog: no release binary available for ${platformKey} at ${RELEASE_VERSION}`;
    void vscode.window.showErrorMessage(msg);
    throw new Error(msg);
  }

  const binDir = getManagedBinDir(context);
  const binPath = getManagedBinPath(context);

  fs.mkdirSync(binDir, { recursive: true });

  return vscode.window.withProgress(
    {
      location: vscode.ProgressLocation.Notification,
      title: `[LazyVerilog] downloading server binary (${RELEASE_VERSION})…`,
      cancellable: false,
    },
    async () => {
      return _downloadAndVerify(platformKey, expectedChecksum, binPath, context);
    },
  );
}

async function _downloadAndVerify(
  platformKey: string,
  expectedChecksum: string,
  binPath: string,
  context: vscode.ExtensionContext,
  isStaticRetry = false,
): Promise<string> {
  const suffix = process.platform === "win32" ? ".exe" : "";
  const asset = `lazyverilog-lsp-${RELEASE_VERSION}-${platformKey}${suffix}`;
  const url = `${RELEASE_BASE_URL}/${RELEASE_VERSION}/${asset}`;
  const tmpPath = `${binPath}.download.${Date.now()}`;

  // Download
  let response: import("http").IncomingMessage;
  try {
    response = await followRedirects(url);
  } catch (e) {
    void vscode.window.showErrorMessage(
      `[LazyVerilog] download failed: ${(e as Error).message}`,
    );
    throw e;
  }

  // Stream to temp file
  await new Promise<void>((resolve, reject) => {
    const out = fs.createWriteStream(tmpPath);
    response.pipe(out);
    out.on("finish", resolve);
    out.on("error", reject);
    response.on("error", (e) => {
      out.destroy();
      reject(e);
    });
  });

  // SHA-256 verify
  const actual = await sha256File(tmpPath);
  if (actual !== expectedChecksum) {
    fs.rmSync(tmpPath, { force: true });
    const msg = `[LazyVerilog] checksum mismatch for ${asset}; expected ${expectedChecksum}, got ${actual}`;
    void vscode.window.showErrorMessage(msg);
    throw new Error(msg);
  }

  // Atomic rename
  fs.renameSync(tmpPath, binPath);

  // chmod +x (non-Windows)
  if (process.platform !== "win32") {
    fs.chmodSync(binPath, 0o755);
  }

  // Linux: check dynamic compatibility, fall back to static
  if (process.platform === "linux" && !isStaticRetry) {
    const compat = await checkLinuxCompat(binPath);
    if (!compat) {
      fs.rmSync(binPath, { force: true });
      void vscode.window.showWarningMessage(
        `[LazyVerilog] binary not compatible (missing libs), trying static build…`,
      );
      const staticKey = `${platformKey}-static`;
      const versionChecksums = RELEASE_CHECKSUMS[RELEASE_VERSION];
      const staticChecksum =
        versionChecksums && staticKey in versionChecksums
          ? versionChecksums[staticKey]
          : undefined;
      if (!staticChecksum) {
        const msg = `[LazyVerilog] no static fallback available for ${staticKey} at ${RELEASE_VERSION}`;
        void vscode.window.showErrorMessage(msg);
        throw new Error(msg);
      }
      return _downloadAndVerify(staticKey, staticChecksum, binPath, context, true);
    }
  }

  void vscode.window.showInformationMessage("[LazyVerilog] server installed");
  return binPath;
}
