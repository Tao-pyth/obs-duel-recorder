from __future__ import annotations

import argparse
import html
import re
import shutil
from pathlib import Path


DEFAULT_OUTPUT = Path("build/docs-site")
SOURCE_DIRS = (Path("docs/user"),)
SOURCE_FILES = (
    Path("README.md"),
    Path("docs/README.md"),
    Path("docs/roadmap.md"),
    Path("docs/release-history.md"),
    Path("docs/pages.md"),
)
EXCLUDED_NAMES = {
    ".git",
    "__pycache__",
    "user_data",
    "logs",
    "db",
    "videos",
    "screenshots",
    "exports",
    "secrets",
}


def main() -> int:
    parser = argparse.ArgumentParser(description="Build the GitHub Pages documentation artifact.")
    parser.add_argument("--root", type=Path, default=Path("."), help="Repository root.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Generated site output directory.")
    args = parser.parse_args()

    root = args.root.resolve()
    output = (root / args.output).resolve()
    _ensure_inside(root, output)
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    for source_dir in SOURCE_DIRS:
        _copy_tree(root / source_dir, output / source_dir)
    for source_file in SOURCE_FILES:
        target = output / source_file
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(root / source_file, target)

    (output / ".nojekyll").write_text("", encoding="utf-8")
    (output / "index.md").write_text(
        "# OBS Duel Recorder Documentation\n\n"
        "- [User Documentation](docs/user/index.md)\n"
        "- [Japanese User Documentation](docs/user/ja/index.md)\n"
        "- [Roadmap](docs/roadmap.md)\n"
        "- [Release History](docs/release-history.md)\n"
        "- [Publication Contract](docs/pages.md)\n",
        encoding="utf-8",
    )
    _render_markdown_site(output)
    _write_manifest(output)
    return 0


def _copy_tree(source: Path, target: Path) -> None:
    if not source.exists():
        raise FileNotFoundError(source)
    for path in source.rglob("*"):
        if any(part in EXCLUDED_NAMES for part in path.parts):
            continue
        relative = path.relative_to(source)
        destination = target / relative
        if path.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)


def _write_manifest(output: Path) -> None:
    files = sorted(
        path.relative_to(output).as_posix()
        for path in output.rglob("*")
        if path.is_file() and path.name != "publication-manifest.txt"
    )
    (output / "publication-manifest.txt").write_text(
        "OBS Duel Recorder GitHub Pages artifact\n\n" + "\n".join(files) + "\n",
        encoding="utf-8",
    )


def _render_markdown_site(output: Path) -> None:
    for markdown_path in output.rglob("*.md"):
        html_path = markdown_path.with_suffix(".html")
        title = _page_title(markdown_path)
        html_path.write_text(
            _html_document(title, _markdown_to_html(markdown_path.read_text(encoding="utf-8"))),
            encoding="utf-8",
        )


def _page_title(markdown_path: Path) -> str:
    for line in markdown_path.read_text(encoding="utf-8").splitlines():
        if line.startswith("# "):
            return line.removeprefix("# ").strip()
    return markdown_path.stem


def _html_document(title: str, body: str) -> str:
    escaped_title = html.escape(title)
    return (
        "<!doctype html>\n"
        "<html lang=\"ja\">\n"
        "<head>\n"
        "  <meta charset=\"utf-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        f"  <title>{escaped_title}</title>\n"
        "  <style>\n"
        "    body{font-family:system-ui,-apple-system,Segoe UI,sans-serif;line-height:1.65;"
        "max-width:960px;margin:0 auto;padding:2rem;color:#24292f;background:#fff;}\n"
        "    a{color:#0969da;} pre{overflow:auto;background:#f6f8fa;padding:1rem;border-radius:6px;}\n"
        "    code{background:#f6f8fa;padding:.12rem .25rem;border-radius:4px;} pre code{padding:0;}\n"
        "    table{border-collapse:collapse;} th,td{border:1px solid #d0d7de;padding:.35rem .55rem;}\n"
        "  </style>\n"
        "</head>\n"
        f"<body>\n{body}\n</body>\n"
        "</html>\n"
    )


def _markdown_to_html(markdown: str) -> str:
    lines = markdown.splitlines()
    output: list[str] = []
    list_open = False
    code_open = False
    paragraph: list[str] = []

    def flush_paragraph() -> None:
        nonlocal paragraph
        if paragraph:
            output.append(f"<p>{_inline_markdown(' '.join(paragraph))}</p>")
            paragraph = []

    def close_list() -> None:
        nonlocal list_open
        if list_open:
            output.append("</ul>")
            list_open = False

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("```"):
            flush_paragraph()
            close_list()
            if code_open:
                output.append("</code></pre>")
                code_open = False
            else:
                output.append("<pre><code>")
                code_open = True
            continue
        if code_open:
            output.append(html.escape(line))
            continue
        if not stripped:
            flush_paragraph()
            close_list()
            continue
        heading = re.match(r"^(#{1,6})\s+(.+)$", stripped)
        if heading:
            flush_paragraph()
            close_list()
            level = len(heading.group(1))
            output.append(f"<h{level}>{_inline_markdown(heading.group(2))}</h{level}>")
            continue
        bullet = re.match(r"^[-*]\s+(.+)$", stripped)
        if bullet:
            flush_paragraph()
            if not list_open:
                output.append("<ul>")
                list_open = True
            output.append(f"<li>{_inline_markdown(bullet.group(1))}</li>")
            continue
        paragraph.append(stripped)

    flush_paragraph()
    close_list()
    if code_open:
        output.append("</code></pre>")
    return "\n".join(output)


def _inline_markdown(text: str) -> str:
    escaped = html.escape(text)
    escaped = re.sub(
        r"\[([^\]]+)\]\(([^)]+\.md)(#[^)]+)?\)",
        lambda match: _html_link(match.group(1), match.group(2), match.group(3)),
        escaped,
    )
    escaped = re.sub(
        r"\[([^\]]+)\]\(([^)]+)\)",
        lambda match: f'<a href="{html.escape(match.group(2), quote=True)}">{match.group(1)}</a>',
        escaped,
    )
    return re.sub(r"`([^`]+)`", r"<code>\1</code>", escaped)


def _html_link(label: str, target: str, anchor: str | None) -> str:
    href = target[:-3] + ".html"
    if anchor:
        href += anchor
    return f'<a href="{html.escape(href, quote=True)}">{label}</a>'


def _ensure_inside(root: Path, target: Path) -> None:
    if target == root or root not in target.parents:
        raise ValueError(f"Output must be inside repository root: {target}")


if __name__ == "__main__":
    raise SystemExit(main())
