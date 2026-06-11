#!/usr/bin/env bash
#
# Release helper for lazyverilog.
#
# This script implements the checksum-lock release flow used by the Neovim
# plugin installer:
#
#   0. Show the user existing release / pre-release tags.
#   1. Ask for the version to release, for example: v1.2.3 or v1.2.3-rc.1.
#   2. Trigger a workflow_dispatch pre-release build for that version.
#   3. Download the per-platform .sha256 artifacts from the pre-release build.
#   4. Update lua/lazyverilog/version.lua and lua/lazyverilog/checksums.lua.
#   5. Commit the release metadata, push the branch, tag the commit, and push
#      the tag to trigger the real GitHub Release build.
#
# Why the two-step build?
#   The plugin must verify downloaded binaries against a checksum list that is
#   committed in the plugin.  The checksums are only known after binaries are
#   built, so the pre-release workflow_dispatch run builds the candidate assets
#   and returns hashes.  The final tag build then rebuilds the assets and fails
#   if any generated hash differs from lua/lazyverilog/checksums.lua.
#
# Assumptions:
#   - Release tags use a leading "v" SemVer-ish spelling, such as v1.0.2.
#   - The version file is lua/lazyverilog/version.lua and contains:
#         return "vX.Y.Z"
#   - The checksum lock is lua/lazyverilog/checksums.lua.
#   - `gh` (GitHub CLI) is installed and authenticated when build testing is on.
#
# Optional flags:
#   --version VERSION   Skip the interactive version prompt.
#   --no-commit        Update files, but do not create a commit.
#   --no-tag           Do not create a git tag.
#   --no-push          Do not run git push / git push --tags.
#   --no-build-test    Skip the workflow_dispatch pre-release build.
#                      The checksum lock must already contain VERSION entries.
#   --dry-run          Print commands that would mutate git, but do not run them.
#
# Examples:
#   tools/release.sh
#   tools/release.sh --version v1.2.3-rc.1 --no-push
#   tools/release.sh --version v1.2.3 --dry-run

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

VERSION_FILE="${REPO_ROOT}/lua/lazyverilog/version.lua"
CHECKSUM_FILE="${REPO_ROOT}/lua/lazyverilog/checksums.lua"
RELEASE_WORKFLOW="release.yml"
RELEASE_REPO="hxxdev/LazyVerilog"

VERSION=""
DO_COMMIT=1
DO_TAG=1
DO_PUSH=1
DO_BUILD_TEST=1
DRY_RUN=0
CHECKSUM_DOWNLOAD_DIR=""

usage() {
    sed -n '2,42p' "$0" | sed 's/^# \{0,1\}//'
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

run() {
    printf '+'
    printf ' %q' "$@"
    printf '\n'

    if [[ "$DRY_RUN" == 1 ]]; then
        return 0
    fi

    "$@"
}

confirm() {
    local prompt="$1"
    local answer

    read -r -p "${prompt} [y/N] " answer
    case "$answer" in
        y|Y|yes|YES) return 0 ;;
        *) return 1 ;;
    esac
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --version)
                [[ $# -ge 2 ]] || die "--version requires an argument"
                VERSION="$2"
                shift 2
                ;;
            --no-commit)
                DO_COMMIT=0
                shift
                ;;
            --no-tag)
                DO_TAG=0
                shift
                ;;
            --no-push)
                DO_PUSH=0
                shift
                ;;
            --no-build-test)
                DO_BUILD_TEST=0
                shift
                ;;
            --dry-run)
                DRY_RUN=1
                shift
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "unknown argument: $1"
                ;;
        esac
    done
}

require_tools() {
    local missing=()
    local tool

    local tools=(git python3)
    if [[ "$DO_BUILD_TEST" == 1 ]]; then
        tools+=(gh)
    fi

    for tool in "${tools[@]}"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing+=("$tool")
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        die "missing required tool(s): ${missing[*]}"
    fi
}

show_existing_versions() {
    printf '\nExisting release / pre-release tags:\n'

    local tags
    tags="$(git -C "$REPO_ROOT" tag --list 'v*' --sort=-v:refname)"

    if [[ -z "$tags" ]]; then
        printf '  (none)\n'
    else
        printf '%s\n' "$tags" | sed 's/^/  /'
    fi
    printf '\n'
}

prompt_version() {
    if [[ -n "$VERSION" ]]; then
        return
    fi

    read -r -p "Version to release (for example v1.2.3 or v1.2.3-rc.1): " VERSION
}

validate_version() {
    [[ -n "$VERSION" ]] || die "version must not be empty"

    if [[ ! "$VERSION" =~ ^v[0-9]+\.[0-9]+\.[0-9]+([-+][0-9A-Za-z][0-9A-Za-z.-]*)?$ ]]; then
        die "version '$VERSION' must look like vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-PRERELEASE"
    fi

    if git -C "$REPO_ROOT" rev-parse -q --verify "refs/tags/${VERSION}" >/dev/null; then
        die "tag '${VERSION}' already exists"
    fi
}

check_release_note() {
    local note="${REPO_ROOT}/docs/releases/${VERSION}.md"
    if [[ -f "$note" ]]; then
        printf 'Release note found: %s\n' "${note#$REPO_ROOT/}"
    else
        printf 'WARNING: no release note found at docs/releases/%s.md\n' "$VERSION"
        confirm "Continue without a release note?" || die "aborted — add docs/releases/${VERSION}.md first"
    fi
}

ensure_clean_enough_for_release() {
    if ! git -C "$REPO_ROOT" diff --quiet || ! git -C "$REPO_ROOT" diff --cached --quiet; then
        printf '\nCurrent git changes:\n'
        git -C "$REPO_ROOT" status --short
        printf '\n'

        if [[ "$DO_COMMIT" == 1 || "$DO_TAG" == 1 || "$DO_PUSH" == 1 ]]; then
            confirm "Continue release with these working-tree changes?" ||
                die "aborted by user"
        fi
    fi
}

update_version_file() {
    [[ -f "$VERSION_FILE" ]] || die "missing version file: $VERSION_FILE"

    printf 'Updating %s -> %s\n' "${VERSION_FILE#$REPO_ROOT/}" "$VERSION"

    local tmp
    tmp="$(mktemp "${VERSION_FILE}.XXXXXX")"
    printf 'return "%s"\n' "$VERSION" > "$tmp"
    mv "$tmp" "$VERSION_FILE"
}

trigger_build_test_and_download_checksums() {
    [[ "$DO_BUILD_TEST" == 1 ]] || return 0

    confirm "Run pre-release build via workflow_dispatch and collect checksums?" ||
        die "aborted before checksum collection"

    local branch
    branch="$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)"

    printf 'Triggering pre-release build for %s via workflow_dispatch on branch %s...\n' "$VERSION" "$branch"
    run gh workflow run "$RELEASE_WORKFLOW" --repo "$RELEASE_REPO" --ref "$branch" -f "version=${VERSION}"

    if [[ "$DRY_RUN" == 1 ]]; then
        return 0
    fi

    # Wait for the run to appear (dispatch is async).  Filter to workflow runs
    # that are not completed so an older completed release run is not selected.
    printf 'Waiting for workflow run to start'
    local run_id=""
    local attempts=0
    while [[ $attempts -lt 20 ]]; do
        printf '.'
        sleep 3
        run_id="$(gh run list --repo "$RELEASE_REPO" --workflow="$RELEASE_WORKFLOW" \
            --limit=10 --json databaseId,status,event,headBranch \
            --jq '.[] | select(.event == "workflow_dispatch" and .headBranch == "'"$branch"'" and .status != "completed") | .databaseId' \
            2>/dev/null | head -n 1 || true)"
        [[ -n "$run_id" ]] && break
        ((attempts++))
    done
    printf '\n'

    [[ -n "$run_id" ]] || die "could not find workflow run after dispatch — check Actions tab manually"

    printf 'Watching run %s...\n' "$run_id"
    gh run watch "$run_id" --repo "$RELEASE_REPO" --exit-status \
        || die "pre-release build failed — fix the issue before releasing"

    CHECKSUM_DOWNLOAD_DIR="$(mktemp -d)"
    printf 'Downloading checksum artifacts to %s...\n' "$CHECKSUM_DOWNLOAD_DIR"
    gh run download "$run_id" --repo "$RELEASE_REPO" \
        --dir "$CHECKSUM_DOWNLOAD_DIR" \
        --pattern "lazyverilog-checksum-${VERSION}-*"

    printf 'Pre-release build passed and checksum artifacts were downloaded.\n\n'
}

update_checksum_lock_from_artifacts() {
    [[ "$DO_BUILD_TEST" == 1 ]] || return 0
    [[ "$DRY_RUN" == 0 ]] || return 0
    [[ -n "$CHECKSUM_DOWNLOAD_DIR" ]] || die "internal error: checksum artifact directory is empty"
    [[ -f "$CHECKSUM_FILE" ]] || die "missing checksum lock file: $CHECKSUM_FILE"

    printf 'Updating %s from downloaded checksum artifacts...\n' "${CHECKSUM_FILE#$REPO_ROOT/}"

    python3 - "$VERSION" "$CHECKSUM_FILE" "$CHECKSUM_DOWNLOAD_DIR" <<'PY'
import pathlib
import re
import sys

version = sys.argv[1]
checksum_file = pathlib.Path(sys.argv[2])
artifact_dir = pathlib.Path(sys.argv[3])

platform_order = [
    "linux-x64",
    "linux-arm64",
    "linux-x64-static",
    "linux-arm64-static",
    "darwin-x64",
    "darwin-arm64",
]

prefix = f"lazyverilog-lsp-{version}-"
suffix = ".sha256"
checksums = {}

for path in artifact_dir.rglob("*.sha256"):
    name = path.name
    if not name.startswith(prefix) or not name.endswith(suffix):
        continue
    platform = name[len(prefix):-len(suffix)]
    digest = path.read_text(encoding="utf-8").split()[0].lower()
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise SystemExit(f"invalid SHA-256 digest in {path}: {digest!r}")
    checksums[platform] = digest

missing = [platform for platform in platform_order if platform not in checksums]
if missing:
    raise SystemExit(
        "missing checksum artifact(s) for: " + ", ".join(missing)
    )

extra = sorted(set(checksums) - set(platform_order))
if extra:
    raise SystemExit("unexpected checksum platform(s): " + ", ".join(extra))

entry_lines = [f'\t["{version}"] = {{']
for platform in platform_order:
    entry_lines.append(f'\t\t["{platform}"] = "{checksums[platform]}",')
entry_lines.append("\t},")
entry = "\n".join(entry_lines)

text = checksum_file.read_text(encoding="utf-8")
version_re = re.compile(
    r'\t\["' + re.escape(version) + r'"\]\s*=\s*\{.*?\n\t\},\n?',
    re.DOTALL,
)

if version_re.search(text):
    text = version_re.sub(entry + "\n", text, count=1)
else:
    marker = "return {\n"
    if marker not in text:
        raise SystemExit("checksum lock file does not contain expected 'return {' marker")
    text = text.replace(marker, marker + entry + "\n\n", 1)

checksum_file.write_text(text, encoding="utf-8")

print(f"updated {checksum_file} for {version}")
PY
}

validate_checksum_lock_entries() {
    [[ "$DRY_RUN" == 0 ]] || return 0
    [[ -f "$CHECKSUM_FILE" ]] || die "missing checksum lock file: $CHECKSUM_FILE"

    python3 - "$VERSION" "$CHECKSUM_FILE" <<'PY'
import pathlib
import re
import sys

version = sys.argv[1]
checksum_file = pathlib.Path(sys.argv[2])
text = checksum_file.read_text(encoding="utf-8")
platforms = [
    "linux-x64",
    "linux-arm64",
    "linux-x64-static",
    "linux-arm64-static",
    "darwin-x64",
    "darwin-arm64",
]

version_re = re.compile(
    r'\["' + re.escape(version) + r'"\]\s*=\s*{(?P<body>.*?)\n\s*},',
    re.DOTALL,
)
match = version_re.search(text)
if not match:
    raise SystemExit(f"checksum lock is missing version {version}")
body = match.group("body")

for platform in platforms:
    platform_re = re.compile(
        r'\["' + re.escape(platform) + r'"\]\s*=\s*"[0-9a-fA-F]{64}"'
    )
    if not platform_re.search(body):
        raise SystemExit(f"checksum lock is missing {version} {platform}")

print(f"checksum lock contains all required entries for {version}")
PY
}

commit_release_files() {
    [[ "$DO_COMMIT" == 1 ]] || return 0

    run git -C "$REPO_ROOT" add "$VERSION_FILE" "$CHECKSUM_FILE"

    local note="${REPO_ROOT}/docs/releases/${VERSION}.md"
    if [[ -f "$note" ]]; then
        run git -C "$REPO_ROOT" add "$note"
    fi

    if git -C "$REPO_ROOT" diff --cached --quiet -- "$VERSION_FILE" "$CHECKSUM_FILE" "$note"; then
        printf 'No staged release metadata changes to commit.\n'
        return 0
    fi

    run git -C "$REPO_ROOT" commit -m "Release ${VERSION}"
}

push_branch() {
    [[ "$DO_PUSH" == 1 ]] || return 0

    confirm "Push branch now?" || die "aborted before pushing branch"
    run git -C "$REPO_ROOT" push
}

tag_release() {
    [[ "$DO_TAG" == 1 ]] || return 0

    if git -C "$REPO_ROOT" rev-parse -q --verify "refs/tags/${VERSION}" >/dev/null; then
        printf 'Tag %s already exists; not creating it again.\n' "$VERSION"
        return 0
    fi

    run git -C "$REPO_ROOT" tag -a "$VERSION" -m "Release ${VERSION}"
}

push_tags() {
    [[ "$DO_PUSH" == 1 && "$DO_TAG" == 1 ]] || return 0

    confirm "Push tag ${VERSION} now? (triggers GitHub Actions release build)" \
        || die "aborted before pushing tag"
    run git -C "$REPO_ROOT" push --tags
}

main() {
    parse_args "$@"
    require_tools

    show_existing_versions
    prompt_version
    validate_version

    printf 'Release version: %s\n' "$VERSION"
    printf '\n'

    check_release_note

    confirm "Proceed with release?" || die "aborted by user"

    ensure_clean_enough_for_release
    trigger_build_test_and_download_checksums
    update_version_file
    update_checksum_lock_from_artifacts
    validate_checksum_lock_entries
    commit_release_files
    push_branch
    tag_release
    push_tags

    printf '\nRelease %s tagged and pushed.\n' "$VERSION"
    printf 'GitHub Actions will verify checksums, build, and upload assets automatically.\n'
    printf 'Monitor: https://github.com/%s/actions\n' "$RELEASE_REPO"
}

main "$@"
