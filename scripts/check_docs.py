#!/usr/bin/env python3
"""文档检查：断链、禁止的绝对路径、索引覆盖。

用法:
    python3 scripts/check_docs.py [--root .]

退出码:
    0 通过；1 发现错误。
"""

from __future__ import annotations

import argparse
import re
import sys
import urllib.parse
from pathlib import Path
from typing import Iterable

SKIP_DIRS = {
    ".git",
    "third_party",
    "build",
    "cmake-build-test",
    "__pycache__",
    ".venv",
}
SKIP_PREFIXES = ("build-", "cmake-build-")

# 历史材料保持原样：其中的 file:/// 绝对链接与旧路径是当时的记录。
HISTORICAL = re.compile(
    r"docs/(history/iterations/ITERATION_\d+.*\.md|history/PROJECT-LOG\.md)$"
)

LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.-]*:")

# 这些缺失不阻断（示例占位路径）。
IGNORED_TARGETS = {"#", ""}

INDEXED_DIRS = (
    "docs/conventions",
    "docs/architecture",
    "docs/decisions",
    "docs/plan",
    "docs/reviews",
    "docs/changes",
)


def iter_markdown(root: Path) -> Iterable[Path]:
    for path in root.rglob("*.md"):
        rel = path.relative_to(root)
        if any(part in SKIP_DIRS for part in rel.parts):
            continue
        if any(part.startswith(SKIP_PREFIXES) for part in rel.parts):
            continue
        yield path


def is_historical(rel: Path) -> bool:
    return bool(HISTORICAL.search(rel.as_posix()))


def clean_target(raw: str) -> str:
    target = raw.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1].strip()
    # 去掉可选的 Markdown title: (path "title")
    if " " in target:
        target = target.split(" ", 1)[0]
    return target


def check_file(path: Path, root: Path) -> list[str]:
    rel = path.relative_to(root)
    text = path.read_text(encoding="utf-8")
    errors: list[str] = []
    if is_historical(rel):
        return errors

    for line_no, line in enumerate(text.splitlines(), start=1):
        for match in LINK_RE.finditer(line):
            target = clean_target(match.group(1))
            if target in IGNORED_TARGETS:
                continue
            if SCHEME_RE.match(target):
                scheme = target.split(":", 1)[0].lower()
                if scheme in ("http", "https", "mailto", "tel"):
                    continue
                errors.append(f"{rel}:{line_no}: 禁止的链接协议 -> {target}")
                continue
            if target.startswith("/") or target.startswith("\\") or re.match(r"^[A-Za-z]:[\\/]", target):
                errors.append(f"{rel}:{line_no}: 禁止的绝对路径 -> {target}")
                continue
            if "\\" in target:
                errors.append(f"{rel}:{line_no}: 请使用 / 而不是 \\ -> {target}")
                continue

            path_part = urllib.parse.unquote(target.split("#", 1)[0])
            if not path_part:
                continue
            resolved = (path.parent / path_part).resolve()
            try:
                resolved.relative_to(root.resolve())
            except ValueError:
                errors.append(f"{rel}:{line_no}: 链接指向仓库外 -> {target}")
                continue
            if not resolved.exists():
                errors.append(f"{rel}:{line_no}: 链接目标不存在 -> {target}")
    return errors


def check_index(root: Path) -> list[str]:
    index = root / "docs" / "INDEX.md"
    if not index.exists():
        return ["docs/INDEX.md 不存在"]
    text = index.read_text(encoding="utf-8")
    errors: list[str] = []
    for dirname in INDEXED_DIRS:
        directory = root / dirname
        if not directory.exists():
            continue
        for md in sorted(directory.glob("*.md")):
            if md.name == "INDEX.md":
                continue
            # 只要索引里以任意相对形式提到该文件名即可（人工索引的写法较灵活）。
            if md.name not in text:
                errors.append(f"docs/INDEX.md 未收录 -> {md.relative_to(root)}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="检查 Markdown 链接与文档索引")
    parser.add_argument("--root", default=".", help="仓库根目录（默认当前目录）")
    args = parser.parse_args()
    root = Path(args.root).resolve()

    errors: list[str] = []
    files = sorted(iter_markdown(root))
    historical = 0
    for path in files:
        rel = path.relative_to(root)
        if is_historical(rel):
            historical += 1
            continue
        errors.extend(check_file(path, root))

    errors.extend(check_index(root))

    if errors:
        print("[check_docs] 失败：")
        for error in errors:
            print(f"  - {error}")
        print(f"\n共 {len(errors)} 个问题，检查 {len(files)} 篇 Markdown（跳过 {historical} 篇历史材料）")
        return 1

    print(
        f"[check_docs] 通过：{len(files)} 篇 Markdown，"
        f"{historical} 篇历史材料已跳过，索引覆盖正常"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
