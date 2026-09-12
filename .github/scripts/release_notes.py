#!/usr/bin/env python3
"""统一格式化 GitHub Release 与飞书使用的分类更新说明。"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


CATEGORY_ORDER = ("新增", "修复", "优化")
CATEGORY_RE = re.compile(
    r"^\s*(?:#{1,6}\s*)?(?:\*\*)?(新增|修复|优化)(?:\*\*)?[：:]?\s*$"
)
NUMBERED_ITEM_RE = re.compile(r"^\s*\d+[.)、]\s*(.+?)\s*$")
VERSION_LINE_RE = re.compile(r"^\s*(?:#{1,6}\s*)?`?v\d+\.\d+\.\d+`?[：:]?\s*$")
IMPORTANT_NOTICE_RE = re.compile(
    r"^\s*(?:>\s*)?(?:\*\*)?重要提示(?:\*\*)?[：:]\s*(.+?)\s*$"
)
MAX_FEISHU_TEXT_CHARS = 4500


def read_text(path: str | None) -> str:
    if path:
        return Path(path).read_text(encoding="utf-8")
    return sys.stdin.read()


def parse_categories(text: str) -> dict[str, list[str]]:
    categories = {name: [] for name in CATEGORY_ORDER}
    current: str | None = None
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or VERSION_LINE_RE.fullmatch(line):
            continue
        category_match = CATEGORY_RE.fullmatch(line)
        if category_match:
            current = category_match.group(1)
            continue
        item_match = NUMBERED_ITEM_RE.fullmatch(line)
        if current and item_match:
            item = " ".join(item_match.group(1).split())
            if item:
                categories[current].append(item)
    return categories


def require_categories(text: str) -> dict[str, list[str]]:
    categories = parse_categories(text)
    if not any(categories.values()):
        raise ValueError("release notes contain no categorized update items")
    return categories


def parse_important_notices(text: str) -> list[str]:
    notices: list[str] = []
    in_github_important_block = False
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if line == "> [!IMPORTANT]":
            in_github_important_block = True
            continue
        if in_github_important_block:
            if line.startswith("> "):
                notice = " ".join(line[2:].split())
                if notice:
                    notices.append(notice)
                continue
            in_github_important_block = False
        match = IMPORTANT_NOTICE_RE.fullmatch(line)
        if not match:
            continue
        notice = " ".join(match.group(1).split())
        if notice:
            notices.append(notice)
    return notices


def format_github_markdown(version: str, text: str) -> str:
    categories = require_categories(text)
    lines = [f"## {version}"]
    notices = parse_important_notices(text)
    if notices:
        lines.extend(("", "> [!IMPORTANT]"))
        lines.extend(f"> {notice}" for notice in notices)
    for name in CATEGORY_ORDER:
        items = categories[name]
        if not items:
            continue
        lines.extend(("", f"### {name}", ""))
        lines.extend(f"{index}. {item}" for index, item in enumerate(items, start=1))
    return "\n".join(lines)


def format_feishu_text(version: str, repository: str, release_url: str, text: str) -> str:
    categories = require_categories(text)
    lines = ["天气时钟 Release 更新", f"版本：{version}"]
    notices = parse_important_notices(text)
    if notices:
        lines.append("重要提示：")
        lines.extend(notices)
    footer = [f"仓库：{repository}", f"Release：{release_url}"]
    truncated = False
    for name in CATEGORY_ORDER:
        items = categories[name]
        if not items:
            continue
        section = [f"{name}（{len(items)}项）："]
        for index, item in enumerate(items, start=1):
            candidate = [
                *lines,
                *section,
                f"{index}. {item}",
                "其余更新内容见 Release。",
                *footer,
            ]
            if len("\n".join(candidate)) > MAX_FEISHU_TEXT_CHARS:
                truncated = True
                break
            section.append(f"{index}. {item}")
        lines.extend(section)
        if truncated:
            break
    if truncated:
        lines.append("其余更新内容见 Release。")
    lines.extend(footer)
    return "\n".join(lines)


def write_payload(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(
            {"msg_type": "text", "content": {"text": text}},
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    markdown = subparsers.add_parser("markdown")
    markdown.add_argument("--version", required=True)
    markdown.add_argument("--input")

    feishu = subparsers.add_parser("feishu-payload")
    feishu.add_argument("--version", required=True)
    feishu.add_argument("--repository", required=True)
    feishu.add_argument("--release-url", required=True)
    feishu.add_argument("--input", required=True)
    feishu.add_argument("--output", required=True, type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if re.fullmatch(r"v\d+\.\d+\.\d+", args.version) is None:
        raise SystemExit(f"invalid version: {args.version}")
    text = read_text(args.input)
    if args.command == "markdown":
        print(format_github_markdown(args.version, text))
        return 0
    release_text = format_feishu_text(
        args.version,
        args.repository,
        args.release_url,
        text,
    )
    write_payload(args.output, release_text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
