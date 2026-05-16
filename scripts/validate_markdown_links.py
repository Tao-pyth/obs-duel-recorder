#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import re
import shlex
import sys
from dataclasses import dataclass


MARKDOWN_LINK_RE = re.compile(r"\[[^\]]*\]\(([^)]+)\)")
FENCED_CODE_BLOCK_RE = re.compile(r"(?s)(```.*?```|~~~.*?~~~)")


@dataclass(frozen=True)
class LinkProblem:
    source_file: pathlib.Path
    target: str
    reason: str


def iter_markdown_files(root: pathlib.Path) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for pattern in ("README*.md", "docs/**/*.md"):
        files.extend(root.glob(pattern))
    # de-duplicate while preserving order
    seen: set[pathlib.Path] = set()
    out: list[pathlib.Path] = []
    for path in files:
        if path in seen:
            continue
        seen.add(path)
        out.append(path)
    return out


def scrub_fenced_code_blocks(text: str) -> str:
    return FENCED_CODE_BLOCK_RE.sub("", text)


def normalize_target(raw_target: str) -> str:
    target = raw_target.strip()

    # ignore non-file targets
    if target.startswith("http://") or target.startswith("https://"):
        return ""
    if target.startswith("mailto:"):
        return ""

    # handle optional title: [text](path "title")
    if target.startswith("<") and ">" in target:
        # Prefer the angle-bracket destination if present.
        angle = target.split(">", 1)[0].lstrip("<").strip()
        if angle:
            target = angle
    else:
        try:
            parts = shlex.split(target, posix=True)
        except ValueError:
            parts = []
        if parts:
            target = parts[0]

    # strip query / fragment
    target = target.split("#", 1)[0].split("?", 1)[0].strip()
    if not target:
        return ""

    # ignore same-page anchors like (#section)
    if target.startswith("#"):
        return ""

    return target


def check_links(root: pathlib.Path) -> list[LinkProblem]:
    problems: list[LinkProblem] = []

    for md_file in iter_markdown_files(root):
        try:
            text = md_file.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            problems.append(
                LinkProblem(md_file, "", "failed to read as utf-8 (non-utf8 markdown)")
            )
            continue

        scrubbed = scrub_fenced_code_blocks(text)

        for match in MARKDOWN_LINK_RE.finditer(scrubbed):
            raw_target = match.group(1)

            # ignore image links: ![alt](...)
            if match.start() > 0 and scrubbed[match.start() - 1] == "!":
                continue

            target = normalize_target(raw_target)
            if not target:
                continue

            # ignore absolute paths and windows drive links
            if target.startswith("/") or re.match(r"^[a-zA-Z]:\\", target):
                continue

            resolved = (md_file.parent / target).resolve()

            # keep checks scoped to repo root
            try:
                resolved.relative_to(root.resolve())
            except ValueError:
                problems.append(
                    LinkProblem(md_file, raw_target, "resolves outside repository")
                )
                continue

            if resolved.exists():
                continue

            # allow implicit .md extension when the target lacks one
            if resolved.suffix == "":
                md_candidate = resolved.with_suffix(".md")
                if md_candidate.exists():
                    continue

            problems.append(LinkProblem(md_file, raw_target, "target not found"))

    return problems


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Validate Markdown file links.")
    parser.add_argument(
        "--root",
        default=".",
        help="Repository root directory (default: .)",
    )

    args = parser.parse_args(argv)

    root = pathlib.Path(args.root).resolve()
    problems = check_links(root)

    if not problems:
        print("OK: no broken Markdown links found")
        return 0

    print(f"Found {len(problems)} problem(s):")
    for problem in problems:
        rel = problem.source_file.relative_to(root)
        target = problem.target or "(n/a)"
        print(f"- {rel}: {target} -> {problem.reason}")

    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
