#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass


INDEX_PATH = pathlib.Path("docs/user/index.md")
JP_INDEX_PATH = pathlib.Path("docs/user/ja/index.md")


CURRENT_DOCS_SECTION_RE = re.compile(
    r"(?ms)^##\s+Current user docs\s+\(English-base, pre-multilingual\)\s*$.*?^(?=##\s+|\Z)"
)
MARKDOWN_LINK_RE = re.compile(r"\[[^\]]+\]\(([^)]+)\)")


@dataclass(frozen=True)
class Problem:
    message: str


def extract_current_english_pages(index_text: str) -> list[str]:
    match = CURRENT_DOCS_SECTION_RE.search(index_text)
    if not match:
        return []

    section = match.group(0)
    pages: list[str] = []

    for m in MARKDOWN_LINK_RE.finditer(section):
        target = m.group(1).strip()
        target = target.split("#", 1)[0].split("?", 1)[0].strip()
        if not target:
            continue
        if target.startswith("http://") or target.startswith("https://"):
            continue
        if target.startswith("mailto:"):
            continue
        pages.append(target)

    # de-duplicate while preserving order
    out: list[str] = []
    seen: set[str] = set()
    for page in pages:
        if page in seen:
            continue
        seen.add(page)
        out.append(page)
    return out


def expected_jp_pages_for(english_rel: str) -> list[str]:
    # v0.2 policy: English is canonical; Japanese may split an English page.
    # Keep this mapping intentionally minimal and conservative.
    if english_rel == "install.md":
        return ["docs/user/ja/install.md"]
    if english_rel == "setup.md":
        return ["docs/user/ja/first-setup.md"]
    if english_rel == "usage.md":
        return ["docs/user/ja/usage.md"]

    # fallback: require a same-name file under ja/
    return [f"docs/user/ja/{pathlib.Path(english_rel).name}"]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Validate Japanese user docs coverage against canonical English-base docs."
    )
    parser.add_argument(
        "--root",
        default=".",
        help="Repository root directory (default: .)",
    )

    args = parser.parse_args(argv)
    root = pathlib.Path(args.root).resolve()

    problems: list[Problem] = []
    warnings: list[str] = []

    index_file = root / INDEX_PATH
    if not index_file.exists():
        print(f"SKIP: {INDEX_PATH.as_posix()} not found")
        return 0

    index_text = index_file.read_text(encoding="utf-8")
    english_pages = extract_current_english_pages(index_text)
    if not english_pages:
        print(
            "SKIP: could not find 'Current user docs (English-base, pre-multilingual)' section"
        )
        return 0

    for english_rel in english_pages:
        expected = expected_jp_pages_for(english_rel)
        missing = [p for p in expected if not (root / p).exists()]
        if missing:
            problems.append(
                Problem(
                    f"Missing Japanese coverage for '{english_rel}': expected {', '.join(missing)}"
                )
            )

    # Best-effort check: Japanese index should link to the expected JP entrypoints.
    jp_index_file = root / JP_INDEX_PATH
    if jp_index_file.exists():
        jp_index_text = jp_index_file.read_text(encoding="utf-8")
        for english_rel in english_pages:
            for jp_page in expected_jp_pages_for(english_rel):
                jp_rel = pathlib.Path(jp_page).name
                if f"({jp_rel})" not in jp_index_text and f"({jp_page})" not in jp_index_text:
                    warnings.append(
                        f"Japanese index may not link to expected page: {JP_INDEX_PATH.as_posix()} -> {jp_rel}"
                    )

    if not problems and not warnings:
        print("OK: Japanese user docs coverage looks consistent")
        return 0

    if warnings:
        print(f"Warnings ({len(warnings)}):")
        for w in warnings:
            print(f"- {w}")

    if problems:
        print(f"Problems ({len(problems)}):")
        for p in problems:
            print(f"- {p.message}")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
