#!/usr/bin/env python3
"""Generate the Nous Engine diagram hub (docs/diagrams/index.html).

Every archify diagram lives in its own folder under docs/diagrams/systems/, and
the hub is derived entirely from what is on disk -- the spec's own meta.title,
its component/connection counts, its guided views, and the visual-check receipt
sitting beside it. There is no second place to keep a title in sync.

Adding a system is therefore: author the spec, deliver it, rerun this script.

    python Tools/build_diagram_hub.py

An optional hub.json beside a spec carries only what the spec cannot express:

    { "group": "Renderer", "order": 10, "blurb": "one line for the card" }
"""

from __future__ import annotations

import html
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DIAGRAMS = REPO_ROOT / "docs" / "diagrams"
SYSTEMS = DIAGRAMS / "systems"
MERMAID = DIAGRAMS / "mermaid"
INDEX = DIAGRAMS / "index.html"

DIAGRAM_TYPES = ("architecture", "workflow", "sequence", "dataflow", "lifecycle")

# The archify component types, mapped to the accent each card's chips use.
TYPE_ACCENT = {
    "architecture": "emerald",
    "dataflow": "cyan",
    "sequence": "violet",
    "workflow": "amber",
    "lifecycle": "rose",
}


class Diagram:
    """One delivered archify diagram, described entirely by files on disk."""

    def __init__(self, spec_path: Path):
        self.spec_path = spec_path
        self.stem = spec_path.name.rsplit(".", 2)[0]
        self.folder = spec_path.parent

        spec = json.loads(spec_path.read_text(encoding="utf-8"))
        meta = spec.get("meta", {})

        # The spec title is standalone ("Nous Engine - X") because the diagram page is
        # opened on its own; on the hub that prefix is on every card, so drop it here.
        title = meta.get("title") or self.stem
        for prefix in ("Nous Engine — ", "Nous Engine - ", "Nous Engine: "):
            if title.startswith(prefix):
                title = title[len(prefix):]
                break
        self.title = title
        self.subtitle = meta.get("subtitle", "")
        self.type = spec.get("diagram_type", "architecture")
        self.revision = (meta.get("repository") or {}).get("revision", "")
        self.views = [v.get("label", "") for v in meta.get("views", []) if v.get("label")]

        # Node/edge wording differs per diagram type; count whichever list exists.
        self.nodes = len(spec.get("components") or spec.get("steps") or
                         spec.get("participants") or spec.get("nodes") or
                         spec.get("states") or [])
        self.edges = len(spec.get("connections") or spec.get("transitions") or
                         spec.get("messages") or spec.get("edges") or
                         spec.get("flows") or [])

        html_path = self.folder / f"{self.stem}.html"
        self.html = html_path if html_path.exists() else None
        self.verified = self._read_visual_check()

        hub = self.folder / "hub.json"
        overrides = json.loads(hub.read_text(encoding="utf-8")) if hub.exists() else {}
        self.group = overrides.get("group", "Systems")
        self.order = overrides.get("order", 100)
        self.blurb = overrides.get("blurb", self.subtitle)

    def _read_visual_check(self) -> str | None:
        """'pass' / 'fail' / None -- None means the check was never run."""
        receipt = self.folder / f"{self.stem}.visual-check.json"
        if not receipt.exists():
            return None
        try:
            return json.loads(receipt.read_text(encoding="utf-8")).get("status")
        except (json.JSONDecodeError, OSError):
            return None

    def rel(self, path: Path) -> str:
        return path.relative_to(DIAGRAMS).as_posix()


def collect() -> list[Diagram]:
    if not SYSTEMS.exists():
        return []
    found = []
    for folder in sorted(p for p in SYSTEMS.iterdir() if p.is_dir()):
        for spec in sorted(folder.glob("*.json")):
            parts = spec.name.split(".")
            # <stem>.<type>.json -- skip visual-check receipts and hub.json.
            if len(parts) == 3 and parts[1] in DIAGRAM_TYPES:
                found.append(Diagram(spec))
    return sorted(found, key=lambda d: (d.group, d.order, d.title))


def card(d: Diagram) -> str:
    e = html.escape
    accent = TYPE_ACCENT.get(d.type, "slate")

    if d.verified == "pass":
        badge = '<span class="badge ok">verified</span>'
    elif d.verified == "fail":
        badge = '<span class="badge bad">check failed</span>'
    else:
        badge = '<span class="badge unknown">not checked</span>'

    chips = "".join(f'<span class="chip">{e(v)}</span>' for v in d.views)
    blurb = f'<p class="blurb">{e(d.blurb)}</p>' if d.blurb else ""
    rev = (f'<a class="meta-link" href="https://github.com/francesctr4/Nous-Engine/tree/{e(d.revision)}"'
           f' target="_blank" rel="noreferrer">@{e(d.revision[:7])}</a>') if d.revision else ""

    if d.html:
        open_link = f'<a class="open" href="{e(d.rel(d.html))}">Open diagram &rarr;</a>'
    else:
        open_link = '<span class="open missing">not delivered yet</span>'

    return f"""      <article class="card {accent}">
        <header>
          <span class="type">{e(d.type)}</span>
          {badge}
        </header>
        <h3>{e(d.title)}</h3>
        {blurb}
        <div class="chips">{chips}</div>
        <dl class="stats">
          <div><dt>nodes</dt><dd>{d.nodes}</dd></div>
          <div><dt>edges</dt><dd>{d.edges}</dd></div>
        </dl>
        <footer>
          {open_link}
          <span class="meta"><a class="meta-link" href="{e(d.rel(d.spec_path))}">spec</a>{rev}</span>
        </footer>
      </article>"""


def mermaid_rows() -> str:
    if not MERMAID.exists():
        return ""
    files = sorted(MERMAID.glob("*.mmd"))
    if not files:
        return ""
    rows = "".join(
        f'<li><a href="mermaid/{html.escape(f.name)}">{html.escape(f.name)}</a>'
        f'<span class="kb">{f.stat().st_size // 1024} KB</span></li>'
        for f in files
    )
    return f"""    <section class="legacy">
      <h2>Mermaid sources</h2>
      <p class="blurb">Earlier hand-written diagrams. Kept as text; not rendered here.</p>
      <ul class="files">{rows}</ul>
    </section>"""


def build() -> str:
    diagrams = collect()
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    if diagrams:
        groups: dict[str, list[Diagram]] = {}
        for d in diagrams:
            groups.setdefault(d.group, []).append(d)
        sections = "\n".join(
            f'    <section>\n      <h2>{html.escape(name)}</h2>\n'
            f'      <div class="grid">\n' + "\n".join(card(d) for d in items) + "\n      </div>\n    </section>"
            for name, items in groups.items()
        )
        verified = sum(1 for d in diagrams if d.verified == "pass")
        summary = f"{len(diagrams)} diagram{'s' if len(diagrams) != 1 else ''} &middot; {verified} verified"
    else:
        sections = ('    <section><p class="blurb">No diagrams yet. Author a spec under '
                    '<code>docs/diagrams/systems/&lt;name&gt;/</code>, deliver it, then rerun '
                    '<code>Tools/build_diagram_hub.py</code>.</p></section>')
        summary = "no diagrams yet"

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Nous Engine &mdash; System Diagrams</title>
<style>
  :root {{
    color-scheme: light dark;
    --bg: #f6f7f9; --panel: #ffffff; --line: #dfe3e8; --ink: #12161c;
    --muted: #5d6672; --accent: #0d9488; --shadow: 0 1px 2px rgba(16,24,40,.06);
    --emerald:#0d9488; --cyan:#0284c7; --violet:#7c3aed; --amber:#b45309; --rose:#be123c; --slate:#475569;
  }}
  :root[data-theme="dark"] {{
    --bg: #0a0d12; --panel: #11161d; --line: #232b36; --ink: #e6edf3;
    --muted: #8b97a6; --accent: #2dd4bf; --shadow: none;
    --emerald:#2dd4bf; --cyan:#38bdf8; --violet:#a78bfa; --amber:#fbbf24; --rose:#fb7185; --slate:#94a3b8;
  }}
  @media (prefers-color-scheme: dark) {{
    :root:not([data-theme="light"]) {{
      --bg: #0a0d12; --panel: #11161d; --line: #232b36; --ink: #e6edf3;
      --muted: #8b97a6; --accent: #2dd4bf; --shadow: none;
      --emerald:#2dd4bf; --cyan:#38bdf8; --violet:#a78bfa; --amber:#fbbf24; --rose:#fb7185; --slate:#94a3b8;
    }}
  }}
  * {{ box-sizing: border-box; }}
  body {{
    margin: 0; background: var(--bg); color: var(--ink);
    font: 14px/1.55 ui-monospace, "JetBrains Mono", "Cascadia Mono", Menlo, Consolas, monospace;
  }}
  .wrap {{ max-width: 1180px; margin: 0 auto; padding: 40px 24px 72px; }}
  header.top {{ display: flex; align-items: baseline; gap: 16px; flex-wrap: wrap; margin-bottom: 8px; }}
  h1 {{ font-size: 22px; margin: 0; letter-spacing: -.01em; }}
  h1 .dot {{ color: var(--accent); }}
  .summary {{ color: var(--muted); font-size: 12px; }}
  .theme {{
    margin-left: auto; background: var(--panel); color: var(--ink); cursor: pointer;
    border: 1px solid var(--line); border-radius: 8px; padding: 6px 12px; font: inherit; font-size: 12px;
  }}
  .theme:hover {{ border-color: var(--accent); }}
  .lede {{ color: var(--muted); margin: 0 0 36px; max-width: 78ch; }}
  h2 {{
    font-size: 12px; text-transform: uppercase; letter-spacing: .09em;
    color: var(--muted); font-weight: 600; margin: 36px 0 14px;
  }}
  .grid {{ display: grid; gap: 16px; grid-template-columns: repeat(auto-fill, minmax(330px, 1fr)); }}
  .card {{
    background: var(--panel); border: 1px solid var(--line); border-radius: 12px;
    padding: 18px; box-shadow: var(--shadow); display: flex; flex-direction: column; gap: 10px;
    border-top: 2px solid var(--slate);
  }}
  .card.emerald {{ border-top-color: var(--emerald); }}
  .card.cyan {{ border-top-color: var(--cyan); }}
  .card.violet {{ border-top-color: var(--violet); }}
  .card.amber {{ border-top-color: var(--amber); }}
  .card.rose {{ border-top-color: var(--rose); }}
  .card header {{ display: flex; align-items: center; gap: 8px; }}
  .type {{ font-size: 11px; letter-spacing: .06em; text-transform: uppercase; color: var(--muted); }}
  .badge {{
    margin-left: auto; font-size: 10px; letter-spacing: .05em; text-transform: uppercase;
    padding: 3px 8px; border-radius: 999px; border: 1px solid var(--line);
  }}
  .badge.ok {{ color: var(--emerald); border-color: color-mix(in srgb, var(--emerald) 40%, transparent); }}
  .badge.bad {{ color: var(--rose); border-color: color-mix(in srgb, var(--rose) 40%, transparent); }}
  .badge.unknown {{ color: var(--muted); }}
  .card h3 {{ margin: 0; font-size: 15px; }}
  .blurb {{ margin: 0; color: var(--muted); font-size: 12.5px; }}
  .chips {{ display: flex; flex-wrap: wrap; gap: 6px; }}
  .chip {{
    font-size: 11px; color: var(--muted); border: 1px solid var(--line);
    border-radius: 6px; padding: 2px 7px;
  }}
  .stats {{ display: flex; gap: 20px; margin: 2px 0 0; }}
  .stats div {{ display: flex; gap: 6px; align-items: baseline; }}
  .stats dt {{ font-size: 11px; color: var(--muted); }}
  .stats dd {{ margin: 0; font-size: 13px; font-variant-numeric: tabular-nums; }}
  .card footer {{
    display: flex; align-items: center; gap: 12px; flex-wrap: wrap;
    margin-top: auto; padding-top: 12px; border-top: 1px solid var(--line);
  }}
  .open {{ color: var(--accent); text-decoration: none; font-size: 13px; }}
  .open:hover {{ text-decoration: underline; }}
  .open.missing {{ color: var(--muted); }}
  .meta {{ margin-left: auto; display: flex; gap: 10px; font-size: 11px; }}
  .meta-link {{ color: var(--muted); text-decoration: none; }}
  .meta-link:hover {{ color: var(--accent); }}
  .files {{ list-style: none; margin: 0; padding: 0; display: grid; gap: 2px; }}
  .files li {{ display: flex; gap: 12px; align-items: baseline; padding: 7px 10px; border-radius: 7px; }}
  .files li:hover {{ background: var(--panel); }}
  .files a {{ color: var(--ink); text-decoration: none; }}
  .files a:hover {{ color: var(--accent); }}
  .kb {{ margin-left: auto; color: var(--muted); font-size: 11px; }}
  code {{ background: var(--panel); border: 1px solid var(--line); border-radius: 4px; padding: 1px 5px; font-size: 12px; }}
  .foot {{ margin-top: 48px; padding-top: 18px; border-top: 1px solid var(--line); color: var(--muted); font-size: 12px; }}
  .foot p {{ margin: 0 0 6px; }}
</style>
</head>
<body>
  <div class="wrap">
    <header class="top">
      <h1><span class="dot">&#9679;</span> Nous Engine &mdash; System Diagrams</h1>
      <span class="summary">{summary}</span>
      <button class="theme" id="theme" type="button">Theme</button>
    </header>
    <p class="lede">
      One explorable diagram per engine system. Each is a self-contained page with pan/zoom,
      search, relationship tracing and export; the <code>SRC</code> badges on a node link to the
      real files at the revision the diagram was pinned to.
    </p>
{sections}
{mermaid_rows()}
    <div class="foot">
      <p>Generated {stamp} by <code>Tools/build_diagram_hub.py</code>. Do not edit this file by hand.</p>
      <p>Add a system: author <code>systems/&lt;name&gt;/&lt;name&gt;.&lt;type&gt;.json</code>, <code>archify deliver</code> it, rerun the script.</p>
    </div>
  </div>
<script>
  (function () {{
    var root = document.documentElement;
    try {{
      var saved = localStorage.getItem("nous-diagram-theme");
      if (saved) root.setAttribute("data-theme", saved);
    }} catch (e) {{}}
    document.getElementById("theme").addEventListener("click", function () {{
      var dark = window.matchMedia("(prefers-color-scheme: dark)").matches;
      var current = root.getAttribute("data-theme") || (dark ? "dark" : "light");
      var next = current === "dark" ? "light" : "dark";
      root.setAttribute("data-theme", next);
      try {{ localStorage.setItem("nous-diagram-theme", next); }} catch (e) {{}}
    }});
  }})();
</script>
</body>
</html>
"""


def main() -> int:
    if not DIAGRAMS.exists():
        print(f"error: {DIAGRAMS} does not exist", file=sys.stderr)
        return 1
    INDEX.write_text(build(), encoding="utf-8")
    diagrams = collect()
    print(f"wrote {INDEX.relative_to(REPO_ROOT).as_posix()} ({len(diagrams)} diagram(s))")
    for d in diagrams:
        state = d.verified or "not checked"
        print(f"  [{state:>11}] {d.group}/{d.title} -- {d.type}, {d.nodes} nodes, {d.edges} edges")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
