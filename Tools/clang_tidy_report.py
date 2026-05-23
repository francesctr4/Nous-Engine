#!/usr/bin/env python3
"""
clang_tidy_report.py  —  Python 3.8+, no external dependencies.

Usage:
    python clang_tidy_report.py                  # whole project
    python clang_tidy_report.py foo.cpp bar.cpp  # specific files
    python clang_tidy_report.py -j 8             # parallel workers
"""

import sys, re, json, subprocess, argparse, shutil
from datetime import datetime
from typing import List, Dict, Set
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from html import escape

# ──────────────────────────── CONFIG ────────────────────────────────
ROOT_DIR  = Path(__file__).resolve().parent.parent   # repo root (tools/../)
CODE_DIR  = ROOT_DIR / "Source"
PROJECT   = ROOT_DIR.name   # used in report title

# Find compile_commands.json under Build/ or Build/<preset>/
candidates = list(ROOT_DIR.glob("Build*/compile_commands.json")) + \
             list(ROOT_DIR.glob("Build*/*/compile_commands.json"))

if not candidates:
    raise RuntimeError("No compile_commands.json found under any Build* directory")

def _build_priority(p: Path) -> tuple:
    name = p.parent.name.lower()
    return (0 if "debug" in name else 1, len(str(p)))

candidates.sort(key=_build_priority)
BUILD_DIR = candidates[0].parent

OUTPUT  = ROOT_DIR / "clang_tidy_report.html"
N_JOBS  = 6
CPP_STD = "c++23"

# Resolve clang-tidy: PATH first, then common install locations
_CLANG_TIDY = shutil.which("clang-tidy") or shutil.which("clang-tidy.exe")
if not _CLANG_TIDY:
    for _c in [
        r"C:\Program Files\LLVM\bin\clang-tidy.exe",
        r"C:\Program Files (x86)\LLVM\bin\clang-tidy.exe",
        "/usr/bin/clang-tidy",
        "/usr/local/bin/clang-tidy",
    ]:
        if Path(_c).exists():
            _CLANG_TIDY = _c
            break
if not _CLANG_TIDY:
    raise RuntimeError("clang-tidy not found. Install LLVM and add its bin/ to PATH.")

print(f"🔧 clang-tidy : {_CLANG_TIDY}")
print(f"📂 Build dir  : {BUILD_DIR}")
# ────────────────────────────────────────────────────────────────────

SEV_RE   = re.compile(r":(\d+):(\d+):\s+(warning|error|note|info):\s+(.*)")
CHECK_RE = re.compile(r'\[([a-z][a-z0-9\-]+)\]\s*$')


# ════════════════════════════════════════════════════════════════════
#  1. PARALLEL ANALYSIS
# ════════════════════════════════════════════════════════════════════

def run_clang_tidy(filepath: Path) -> str:
    try:
        r = subprocess.run(
            [_CLANG_TIDY, str(filepath), "-p", str(BUILD_DIR),
             f"--extra-arg=-std={CPP_STD}"],
            capture_output=True, text=True,
        )
        return r.stdout
    except Exception as e:
        return f"# ERROR running clang-tidy on {filepath}: {e}\n"


def _parse_output(output: str, code_prefix: str) -> Dict[str, List[dict]]:
    """
    Stateful parser: collects diagnostic lines and the code-context lines
    (the '|' lines that clang-tidy appends after each diagnostic).
    Returns {rel_file: [entry, ...]}
    """
    result: Dict[str, List[dict]] = {}
    current_entry: dict | None = None
    ctx_lines: list[str] = []

    def flush():
        nonlocal current_entry, ctx_lines
        if current_entry is None:
            return
        current_entry["ctx"] = "\n".join(ctx_lines)
        result.setdefault(current_entry["file"], []).append(current_entry)
        current_entry = None
        ctx_lines = []

    for raw in output.splitlines():
        norm = raw.replace("\\", "/")

        if code_prefix in norm:
            flush()
            rel_line = norm.split(code_prefix, 1)[1]
            m = SEV_RE.search(rel_line)
            if not m:
                continue
            rel_file = rel_line.split(":")[0]
            msg = m.group(4)
            cm = CHECK_RE.search(msg)
            check = cm.group(1) if cm else ""
            if cm:
                msg = msg[:cm.start()].rstrip()
            current_entry = {
                "line":  m.group(1),
                "col":   m.group(2),
                "sev":   m.group(3),
                "msg":   msg,
                "check": check,
                "file":  rel_file,
            }
        elif current_entry is not None and "|" in raw:
            ctx_lines.append(raw)
        elif current_entry is not None and raw.strip() == "":
            pass   # blank separator — keep accumulating
        else:
            flush()

    flush()
    return result


def analyze_files(files: List[Path], n_jobs: int = N_JOBS) -> Dict[str, List[dict]]:
    warnings: Dict[str, List[dict]] = {}
    total       = len(files)
    code_prefix = str(CODE_DIR).replace("\\", "/") + "/"

    print(f"⏳ Analyzing {total} file(s) with {n_jobs} parallel jobs...")

    with ThreadPoolExecutor(max_workers=n_jobs) as pool:
        future_to_file = {pool.submit(run_clang_tidy, f): f for f in files}
        done = 0
        for future in as_completed(future_to_file):
            done += 1
            filepath = future_to_file[future]
            print(f"\r🔍 [{done}/{total}] {filepath.name:<40}", end="", flush=True)
            for rel_file, entries in _parse_output(future.result(), code_prefix).items():
                warnings.setdefault(rel_file, []).extend(entries)

    print("\n✅ Analysis complete.")
    return warnings


# ════════════════════════════════════════════════════════════════════
#  2. DATA BUILDERS
# ════════════════════════════════════════════════════════════════════

def count_sevs(entries: List[dict]) -> Dict[str, int]:
    c = {"error": 0, "warning": 0, "note": 0, "info": 0}
    for e in entries:
        if e["sev"] in c:
            c[e["sev"]] += 1
    return c


def build_summary_data(warnings: Dict[str, List[dict]]) -> List[dict]:
    rows = []
    for rel_file, entries in warnings.items():
        c = count_sevs(entries)
        rows.append({
            "file":     rel_file,
            "id":       "file_" + re.sub(r"[/.]", "_", rel_file),
            "errors":   c["error"],
            "warnings": c["warning"],
            "notes":    c["note"],
            "info":     c["info"],
            "total":    len(entries),
        })
    rows.sort(key=lambda r: (-r["errors"], -r["warnings"], -r["notes"], -r["info"], r["file"]))
    return rows


def build_checks_data(warnings: Dict[str, List[dict]]) -> List[dict]:
    """Aggregate diagnostics by check name for the By-Check tab."""
    agg: Dict[str, dict] = {}
    for rel_file, entries in warnings.items():
        for e in entries:
            key = e.get("check") or "(unknown)"
            if key not in agg:
                cat = key.split("-")[0] if "-" in key else key
                agg[key] = {"check": key, "category": cat,
                            "total": 0, "errors": 0, "warnings": 0,
                            "notes": 0, "info": 0, "files": set()}
            agg[key]["total"] += 1
            sev = e["sev"]
            if sev in agg[key]:
                agg[key][sev] += 1
            agg[key]["files"].add(rel_file)

    rows = []
    for data in agg.values():
        rows.append({
            "check":    data["check"],
            "category": data["category"],
            "total":    data["total"],
            "errors":   data["errors"],
            "warnings": data["warnings"],
            "notes":    data["notes"],
            "info":     data["info"],
            "nfiles":   len(data["files"]),
        })
    rows.sort(key=lambda r: (-r["errors"], -r["warnings"], -r["total"]))
    return rows


# ════════════════════════════════════════════════════════════════════
#  3. SIDEBAR HTML
# ════════════════════════════════════════════════════════════════════

def build_tree_html(all_files: List[Path], files_with_warnings: Set[str]) -> str:
    tree: dict = {}
    for f in sorted(all_files):
        rel = str(f.relative_to(CODE_DIR)).replace("\\", "/")
        if rel not in files_with_warnings:
            continue
        parts = Path(rel).parts
        node = tree
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        node[parts[-1]] = rel

    def render(node: dict) -> str:
        out = ""
        for key, val in sorted(node.items()):
            if isinstance(val, dict):
                inner = render(val)
                if inner:
                    out += f'<details open><summary class="folder">{escape(key)}</summary>{inner}</details>'
            else:
                fid  = "file_" + re.sub(r"[/.]", "_", val)
                fesc = escape(val)
                out += (
                    f'<a class="filecpp" data-file="{fid}" href="#"'
                    f' onclick="loadFile(\'{fid}\',\'{fesc}\');return false;">'
                    f'{escape(key)}</a>'
                )
        return out

    return render(tree)


def build_sidebar_summary_html(rows: List[dict]) -> str:
    parts = []
    for r in rows:
        badges = ""
        if r["errors"]:   badges += f'<span style="color:#e74c3c">❌{r["errors"]}</span> '
        if r["warnings"]: badges += f'<span style="color:#b8860b">⚠️{r["warnings"]}</span> '
        if r["notes"]:    badges += f'<span style="color:#2980b9">📝{r["notes"]}</span>'
        name = escape(Path(r["file"]).name)
        fid  = r["id"]
        fesc = escape(r["file"])
        parts.append(
            f'<a class="filecpp" data-file="{fid}" href="#"'
            f' onclick="loadFile(\'{fid}\',\'{fesc}\');return false;"'
            f' title="{fesc}">{name} {badges}</a>'
        )
    return "\n".join(parts)


# ════════════════════════════════════════════════════════════════════
#  4. HTML GENERATION
#     JSON is injected via string concatenation — never through
#     str.format() to avoid brace conflicts.
# ════════════════════════════════════════════════════════════════════

CSS = """
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: Arial, sans-serif; background: #f0f2f5; }

/* ── Sidebar ── */
.sidebar {
    width: 290px; background: #1e1e2e; color: #cdd6f4;
    padding: 12px; height: 100vh; overflow-y: auto;
    position: fixed; left: 0; top: 0;
}
.sidebar-header { display: flex; flex-wrap: wrap; gap: 5px; margin-bottom: 10px; align-items: center; }
.sidebar-header h2 { color: white; font-size: 1rem; flex: 1; min-width: 80px; }
.tab-btn {
    background: #313244; color: #cdd6f4; border: none; border-radius: 4px;
    padding: 4px 8px; cursor: pointer; font-size: 0.75rem; white-space: nowrap;
}
.tab-btn.active { background: #cdd6f4; color: #1e1e2e; font-weight: bold; }
#searchBox {
    width: 100%; padding: 6px 8px; border-radius: 4px;
    border: none; margin-bottom: 10px; font-size: 0.9rem;
    background: #313244; color: #cdd6f4;
}
#searchBox::placeholder { color: #6c7086; }
.sidebar a {
    color: #cdd6f4; text-decoration: none; display: block;
    padding: 3px 4px; border-radius: 4px; font-size: 0.82rem;
    white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.sidebar a:hover  { background: #313244; }
.sidebar a.active { background: #cdd6f4 !important; color: #1e1e2e !important; font-weight: bold; }
.folder::before  { content: "📁 "; }
.filecpp::before { content: "📄 "; }
details > summary { cursor: pointer; padding: 3px 0; color: #a6adc8; font-size: 0.82rem; }
details { padding-left: 10px; }

/* ── Content ── */
.content { margin-left: 302px; padding: 24px; min-height: 100vh; }
.placeholder { color: #999; margin-top: 60px; text-align: center; font-size: 1.1rem; }
h2 { margin-bottom: 10px; color: #333; border-bottom: 2px solid #ddd; padding-bottom: 6px; }

/* ── Report header ── */
.report-header {
    background: #1e1e2e; color: #cdd6f4; border-radius: 8px;
    padding: 18px 24px; margin-bottom: 24px;
    display: flex; justify-content: space-between; align-items: center;
    box-shadow: 0 2px 6px rgba(0,0,0,.15);
}
.report-header h1 { font-size: 1.3rem; color: white; }
.report-header .meta { font-size: 0.8rem; color: #a6adc8; text-align: right; line-height: 1.7; }

/* ── Entries ── */
.entry { padding: 10px 14px; margin-bottom: 10px; border-radius: 6px; font-size: 0.9rem; line-height: 1.5; }
.entry.error   { border-left: 5px solid #f38ba8; background: #fff0f0; }
.entry.warning { border-left: 5px solid #f9e2af; background: #fffbf0; }
.entry.note    { border-left: 5px solid #89b4fa; background: #f0f5ff; }
.entry.info    { border-left: 5px solid #a6e3a1; background: #f0fff4; }
.entry.hidden  { display: none; }
.entry-header  { display: flex; align-items: baseline; gap: 8px; flex-wrap: wrap; margin-bottom: 4px; }
.entry-loc     { font-weight: bold; font-size: 0.88rem; color: #555; }
.entry-msg     { flex: 1; }

/* ── Code context ── */
.code-ctx {
    margin-top: 8px; padding: 8px 10px;
    background: #1e1e2e; color: #cdd6f4;
    border-radius: 4px; font-family: 'Consolas','Courier New',monospace;
    font-size: 0.82rem; line-height: 1.5; overflow-x: auto;
    white-space: pre;
}

/* ── Check badges ── */
.check-badge {
    display: inline-block; padding: 1px 7px; border-radius: 10px;
    font-size: 0.75rem; font-weight: bold; white-space: nowrap;
    font-family: 'Consolas','Courier New',monospace;
}
.cat-bugprone          { background: #fce4e4; color: #c0392b; }
.cat-modernize         { background: #e4eeff; color: #2471a3; }
.cat-performance       { background: #fef3e2; color: #d35400; }
.cat-readability       { background: #f3e4fc; color: #7d3c98; }
.cat-cppcoreguidelines { background: #e4f9f5; color: #1a7a5e; }
.cat-cert              { background: #fde8d8; color: #a93226; }
.cat-misc              { background: #eaecee; color: #555; }
.cat-hicpp             { background: #e9f5e9; color: #2e7d32; }
.cat-google            { background: #e8f8e8; color: #27ae60; }
.cat-clang-analyzer    { background: #fef9c3; color: #9a7d0a; }
.cat-unknown           { background: #f0f0f0; color: #999; }

/* ── Filter buttons ── */
.filter-bar { margin-bottom: 14px; display: flex; gap: 8px; flex-wrap: wrap; align-items: center; }
.filter-bar button {
    padding: 5px 10px; border: none; cursor: pointer;
    border-radius: 4px; font-weight: bold; background: #eee; font-size: 0.82rem;
}
.filter-bar button:hover { background: #ddd; }
.filter-bar input {
    padding: 5px 9px; border: 1px solid #ddd; border-radius: 4px; font-size: 0.82rem;
    flex: 1; min-width: 160px; max-width: 300px;
}

/* ── Spinner ── */
.spinner {
    width: 32px; height: 32px; border: 4px solid #ccc; border-top-color: #555;
    border-radius: 50%; animation: spin .7s linear infinite; margin: 60px auto; display: block;
}
@keyframes spin { to { transform: rotate(360deg); } }

/* ── Summary tables ── */
.summary-table {
    width: 100%; border-collapse: collapse; font-size: 0.9rem;
    background: white; border-radius: 6px; overflow: hidden;
    box-shadow: 0 1px 4px rgba(0,0,0,.1);
}
.summary-table th {
    background: #1e1e2e; color: #cdd6f4; padding: 10px 12px;
    text-align: left; font-weight: bold; cursor: pointer; user-select: none;
}
.summary-table th:hover { background: #313244; }
.summary-table td { padding: 8px 12px; border-bottom: 1px solid #eee; vertical-align: middle; }
.summary-table tr:last-child td { border-bottom: none; }
.summary-table tr:hover td { background: #f9f9f9; }
.summary-table td.clickable {
    cursor: pointer; color: #2980b9;
    max-width: 360px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
}
.summary-table td.clickable:hover { color: #1a5e8a; text-decoration: underline; }
.badge {
    display: inline-block; padding: 2px 8px; border-radius: 10px;
    font-weight: bold; font-size: 0.8rem; min-width: 30px; text-align: center;
}
.badge.error   { background: #fce4e4; color: #e74c3c; }
.badge.warning { background: #fff3cd; color: #b8860b; }
.badge.note    { background: #e4eeff; color: #2980b9; }
.badge.info    { background: #e9fbe9; color: #27ae60; }
.badge.zero    { background: #f0f0f0; color: #ccc; }

/* ── Stat cards ── */
.summary-stats { display: flex; gap: 16px; margin-bottom: 24px; flex-wrap: wrap; }
.stat-card {
    background: white; border-radius: 8px; padding: 16px 24px;
    box-shadow: 0 1px 4px rgba(0,0,0,.1); flex: 1; min-width: 110px; text-align: center;
}
.stat-card .num { font-size: 2.2rem; font-weight: bold; line-height: 1; }
.stat-card .lbl { font-size: 0.78rem; color: #888; margin-top: 4px; }
.stat-card.ec .num { color: #e74c3c; }
.stat-card.wc .num { color: #b8860b; }
.stat-card.nc .num { color: #2980b9; }
.stat-card.fc .num { color: #555; }
.stat-card.kc .num { color: #7d3c98; }
"""

JS = r"""
// DATA, SUMMARY and CHECKS are injected just before this block

// ── Category → CSS class ─────────────────────────────────────────
var CAT_CLASS = {
    bugprone: 'cat-bugprone', modernize: 'cat-modernize',
    performance: 'cat-performance', readability: 'cat-readability',
    cppcoreguidelines: 'cat-cppcoreguidelines', cert: 'cat-cert',
    misc: 'cat-misc', hicpp: 'cat-hicpp', google: 'cat-google',
    'clang-analyzer': 'cat-clang-analyzer',
};
function checkBadge(check) {
    if (!check) return '';
    var cat  = check.split('-')[0];
    var cls  = CAT_CLASS[cat] || 'cat-unknown';
    return '<span class="check-badge ' + cls + '">' + escHtml(check) + '</span>';
}

// ── Tabs sidebar ─────────────────────────────────────────────────
function showTab(tab) {
    ['files','summary','checks'].forEach(function(t) {
        document.getElementById('tab-' + t).style.display = tab === t ? '' : 'none';
        document.getElementById('btn-' + t).classList.toggle('active', tab === t);
    });
    if (tab === 'summary') renderSummaryPage();
    if (tab === 'checks')  renderChecksPage();
}

// ── Load file ────────────────────────────────────────────────────
function loadFile(fileId, relPath) {
    document.querySelectorAll('.sidebar a').forEach(function(a) { a.classList.remove('active'); });
    var link = document.querySelector('[data-file="' + fileId + '"]');
    if (link) link.classList.add('active');

    var container = document.getElementById('mainContent');
    container.innerHTML = '<div class="spinner"></div>';

    requestAnimationFrame(function() {
        var entries = DATA[relPath];
        if (!entries || entries.length === 0) {
            container.innerHTML = '<h2>' + escHtml(relPath) + '</h2>'
                + '<p style="color:#888;margin-top:16px">No warnings.</p>';
            return;
        }
        container.innerHTML = buildFileHTML(fileId, relPath, entries);
    });
}

// ── Build file HTML ──────────────────────────────────────────────
function buildFileHTML(fileId, relPath, entries) {
    var counts = { warning: 0, error: 0, note: 0, info: 0 };
    entries.forEach(function(e) { if (counts[e.sev] !== undefined) counts[e.sev]++; });

    function btn(type, icon, label, color) {
        return '<button onclick="filterLocal(\''+fileId+'\',\''+type+'\')" style="color:'+color+'">'
            + icon + ' ' + counts[type] + ' ' + label + '</button>';
    }

    var html = '<div class="file-content" id="' + fileId + '">'
        + '<h2>' + escHtml(relPath) + '</h2>'
        + '<div class="filter-bar">'
        + '<button onclick="filterLocal(\''+fileId+'\',\'all\')">All</button>'
        + btn('error',   '❌', 'errors',   '#e74c3c')
        + btn('warning', '⚠️', 'warnings', '#e6a817')
        + btn('note',    '📝', 'notes',    '#3498db')
        + btn('info',    'ℹ️', 'info',     '#2ecc71')
        + '<input type="text" placeholder="🔍 filter message..." oninput="filterMsg(\''+fileId+'\',this.value)">'
        + '</div>';

    entries.forEach(function(e) {
        var ctx = e.ctx ? '<pre class="code-ctx">' + escHtml(e.ctx) + '</pre>' : '';
        html += '<div class="entry ' + e.sev + '" data-msg="' + escHtml(e.msg.toLowerCase()) + '">'
            + '<div class="entry-header">'
            + '<span class="entry-loc">Line ' + escHtml(e.line) + ':' + escHtml(e.col) + '</span>'
            + checkBadge(e.check)
            + '</div>'
            + '<div class="entry-msg">' + escHtml(e.msg) + '</div>'
            + ctx
            + '</div>';
    });
    html += '</div>';
    return html;
}

// ── Severity filter ──────────────────────────────────────────────
window.filterLocal = function(fileId, type) {
    document.querySelectorAll('#' + fileId + ' .entry').forEach(function(el) {
        el.classList.toggle('hidden', type !== 'all' && !el.classList.contains(type));
    });
};

// ── Message text filter ──────────────────────────────────────────
window.filterMsg = function(fileId, q) {
    q = q.toLowerCase();
    document.querySelectorAll('#' + fileId + ' .entry').forEach(function(el) {
        el.classList.toggle('hidden', q !== '' && !(el.dataset.msg || '').includes(q));
    });
};

// ── Summary page ─────────────────────────────────────────────────
var summaryRendered = false;
function renderSummaryPage() {
    if (summaryRendered) return;
    summaryRendered = true;

    var totalErrors   = SUMMARY.reduce(function(a,r){ return a+r.errors;   }, 0);
    var totalWarnings = SUMMARY.reduce(function(a,r){ return a+r.warnings; }, 0);
    var totalNotes    = SUMMARY.reduce(function(a,r){ return a+r.notes;    }, 0);
    var totalFiles    = SUMMARY.length;

    function badge(val, cls) {
        return '<span class="badge ' + (val > 0 ? cls : 'zero') + '">' + val + '</span>';
    }

    var rows = '';
    SUMMARY.forEach(function(r) {
        rows += '<tr>'
            + '<td class="clickable" onclick="loadFile(\''+r.id+'\',\''+escHtml(r.file)+'\')" title="'+escHtml(r.file)+'">'
            + escHtml(r.file) + '</td>'
            + '<td>' + badge(r.errors,   'error')   + '</td>'
            + '<td>' + badge(r.warnings, 'warning') + '</td>'
            + '<td>' + badge(r.notes,    'note')    + '</td>'
            + '<td>' + badge(r.info,     'info')    + '</td>'
            + '<td><b>' + r.total + '</b></td>'
            + '</tr>';
    });

    document.getElementById('mainContent').innerHTML =
        '<h2 style="margin-bottom:18px">Project Summary</h2>'
        + '<div class="summary-stats">'
        + '<div class="stat-card fc"><div class="num">' + totalFiles    + '</div><div class="lbl">Files with warnings</div></div>'
        + '<div class="stat-card ec"><div class="num">' + totalErrors   + '</div><div class="lbl">Total errors</div></div>'
        + '<div class="stat-card wc"><div class="num">' + totalWarnings + '</div><div class="lbl">Total warnings</div></div>'
        + '<div class="stat-card nc"><div class="num">' + totalNotes    + '</div><div class="lbl">Total notes</div></div>'
        + '</div>'
        + '<table class="summary-table" id="summaryTable"><thead><tr>'
        + '<th onclick="sortTable(\'summaryTable\',0)">File ↕</th>'
        + '<th onclick="sortTable(\'summaryTable\',1)">❌ Errors ↕</th>'
        + '<th onclick="sortTable(\'summaryTable\',2)">⚠️ Warnings ↕</th>'
        + '<th onclick="sortTable(\'summaryTable\',3)">📝 Notes ↕</th>'
        + '<th onclick="sortTable(\'summaryTable\',4)">ℹ️ Info ↕</th>'
        + '<th onclick="sortTable(\'summaryTable\',5)">Total ↕</th>'
        + '</tr></thead><tbody>' + rows + '</tbody></table>';
}

// ── Checks page ──────────────────────────────────────────────────
var checksRendered = false;
function renderChecksPage() {
    if (checksRendered) return;
    checksRendered = true;

    var totalChecks = CHECKS.length;
    var totalHits   = CHECKS.reduce(function(a,r){ return a+r.total; }, 0);

    function badge(val, cls) {
        return '<span class="badge ' + (val > 0 ? cls : 'zero') + '">' + val + '</span>';
    }

    var rows = '';
    CHECKS.forEach(function(r) {
        rows += '<tr>'
            + '<td style="font-family:monospace;font-size:0.85rem">' + checkBadge(r.check) + '</td>'
            + '<td>' + badge(r.errors,   'error')   + '</td>'
            + '<td>' + badge(r.warnings, 'warning') + '</td>'
            + '<td>' + badge(r.notes,    'note')    + '</td>'
            + '<td><b>' + r.total + '</b></td>'
            + '<td style="color:#888">' + r.nfiles + '</td>'
            + '</tr>';
    });

    document.getElementById('mainContent').innerHTML =
        '<h2 style="margin-bottom:18px">By Check</h2>'
        + '<div class="summary-stats">'
        + '<div class="stat-card kc"><div class="num">' + totalChecks + '</div><div class="lbl">Unique checks</div></div>'
        + '<div class="stat-card fc"><div class="num">' + totalHits   + '</div><div class="lbl">Total hits</div></div>'
        + '</div>'
        + '<table class="summary-table" id="checksTable"><thead><tr>'
        + '<th onclick="sortTable(\'checksTable\',0)">Check ↕</th>'
        + '<th onclick="sortTable(\'checksTable\',1)">❌ Errors ↕</th>'
        + '<th onclick="sortTable(\'checksTable\',2)">⚠️ Warnings ↕</th>'
        + '<th onclick="sortTable(\'checksTable\',3)">📝 Notes ↕</th>'
        + '<th onclick="sortTable(\'checksTable\',4)">Total ↕</th>'
        + '<th onclick="sortTable(\'checksTable\',5)">Files ↕</th>'
        + '</tr></thead><tbody>' + rows + '</tbody></table>';
}

// ── Sort table ───────────────────────────────────────────────────
var sortDirs = {};
function sortTable(tableId, colIdx) {
    var tbody = document.querySelector('#' + tableId + ' tbody');
    var rows  = Array.from(tbody.querySelectorAll('tr'));
    var key   = tableId + ':' + colIdx;
    var asc   = !sortDirs[key];
    sortDirs  = {};
    sortDirs[key] = asc;
    rows.sort(function(a, b) {
        var av = a.cells[colIdx].textContent.trim();
        var bv = b.cells[colIdx].textContent.trim();
        var an = parseFloat(av), bn = parseFloat(bv);
        var cmp = isNaN(an) ? av.localeCompare(bv) : an - bn;
        return asc ? cmp : -cmp;
    });
    rows.forEach(function(r) { tbody.appendChild(r); });
}

// ── File search (sidebar) ────────────────────────────────────────
document.getElementById('searchBox').addEventListener('input', function() {
    var q = this.value.toLowerCase();
    document.querySelectorAll('#tab-files a.filecpp').forEach(function(a) {
        a.style.display = q === '' || a.textContent.toLowerCase().includes(q) ? '' : 'none';
    });
    document.querySelectorAll('#tab-files details').forEach(function(det) {
        if (q === '') { det.open = true; return; }
        det.open = Array.from(det.querySelectorAll('a.filecpp'))
            .some(function(a) { return a.style.display !== 'none'; });
    });
});

// ── HTML escape ──────────────────────────────────────────────────
function escHtml(str) {
    return String(str)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;')
        .replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}
"""


def generate_html(all_files: List[Path], warnings: Dict[str, List[dict]]) -> str:
    tree_html    = build_tree_html(all_files, set(warnings.keys()))
    summary_rows = build_summary_data(warnings)
    checks_rows  = build_checks_data(warnings)
    sidebar_sum  = build_sidebar_summary_html(summary_rows)

    data_json    = json.dumps(warnings,      ensure_ascii=False)
    summary_json = json.dumps(summary_rows,  ensure_ascii=False)
    checks_json  = json.dumps(checks_rows,   ensure_ascii=False)

    n_warn     = len(warnings)
    timestamp  = datetime.now().strftime("%Y-%m-%d %H:%M")
    build_name = BUILD_DIR.name

    html = (
        '<!DOCTYPE html>\n'
        '<html lang="en">\n'
        '<head>\n'
        '<meta charset="UTF-8">\n'
        f'<title>Clang-Tidy — {escape(PROJECT)}</title>\n'
        '<style>\n' + CSS + '\n</style>\n'
        '</head>\n'
        '<body>\n'

        '<div class="sidebar">\n'
        '  <div class="sidebar-header">\n'
        f'   <h2>🔍 {escape(PROJECT)}</h2>\n'
        '    <button class="tab-btn active" id="btn-files"   onclick="showTab(\'files\')">📄 Files</button>\n'
        '    <button class="tab-btn"        id="btn-summary" onclick="showTab(\'summary\')">📊 Summary</button>\n'
        '    <button class="tab-btn"        id="btn-checks"  onclick="showTab(\'checks\')">🏷 Checks</button>\n'
        '  </div>\n'

        '  <div id="tab-files">\n'
        '    <input id="searchBox" placeholder="🔍 Filter files..." autocomplete="off">\n'
        + tree_html +
        '  </div>\n'

        '  <div id="tab-summary" style="display:none">\n'
        f'    <p style="color:#a6adc8;font-size:0.75rem;padding:2px 0 8px">{n_warn} files with warnings</p>\n'
        + sidebar_sum +
        '  </div>\n'

        '  <div id="tab-checks" style="display:none">\n'
        f'    <p style="color:#a6adc8;font-size:0.75rem;padding:2px 0 8px">'
        f'{len(checks_rows)} unique checks — click 🏷 Checks</p>\n'
        '  </div>\n'
        '</div>\n'

        '<div class="content">\n'
        '  <div class="report-header">\n'
        f'    <div><h1>Clang-Tidy Report — {escape(PROJECT)}</h1></div>\n'
        f'    <div class="meta">Build: <b>{escape(build_name)}</b><br>Generated: {timestamp}</div>\n'
        '  </div>\n'
        '  <div id="mainContent">\n'
        '    <p class="placeholder">👈 Select a file from the index<br><br>'
        'or open <b>📊 Summary</b> / <b>🏷 Checks</b> for project-wide views.</p>\n'
        '  </div>\n'
        '</div>\n'

        '<script>\n'
        'const DATA    = ' + data_json    + ';\n'
        'const SUMMARY = ' + summary_json + ';\n'
        'const CHECKS  = ' + checks_json  + ';\n'
        + JS +
        '</script>\n'
        '</body>\n'
        '</html>\n'
    )
    return html


# ════════════════════════════════════════════════════════════════════
#  5. MAIN
# ════════════════════════════════════════════════════════════════════

def collect_files(args: List[str]) -> List[Path]:
    if args:
        found = []
        for name in args:
            if not name.endswith(".cpp"):
                continue
            matches = list(CODE_DIR.rglob(name))
            if matches:
                print(f"✔  Found: {matches[0]}")
                found.extend(matches)
            else:
                print(f"✘  Not found: {name}")
        return found
    print(f"📌 Analyzing entire project under {CODE_DIR}")
    return list(CODE_DIR.rglob("*.cpp"))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate an HTML clang-tidy report with lazy loading."
    )
    parser.add_argument("files", nargs="*",
        help=".cpp files to analyze (omit for entire project).")
    parser.add_argument("--jobs", "-j", type=int, default=N_JOBS,
        help=f"Parallel workers (default: {N_JOBS}).")
    opts = parser.parse_args()

    files = collect_files(opts.files)
    if not files:
        print("No .cpp files found. Aborting.")
        sys.exit(1)

    warnings  = analyze_files(files, opts.jobs)
    all_files = sorted(CODE_DIR.rglob("*.cpp"))

    n_checks = len(build_checks_data(warnings))
    print(f"📝 Generating HTML  ({len(warnings)} files · {n_checks} unique checks)...")
    OUTPUT.write_text(generate_html(all_files, warnings), encoding="utf-8")
    print("=" * 55)
    print(f"  ✔  Report: {OUTPUT.resolve()}")
    print("=" * 55)


if __name__ == "__main__":
    main()
