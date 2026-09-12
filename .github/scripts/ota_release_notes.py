#!/usr/bin/env python3
"""把完整 Release 说明压缩为设备 OTA 清单使用的短摘要。"""

from __future__ import annotations

import re
import sys
import tempfile
from pathlib import Path


MAX_OTA_NOTES_BYTES = 768
MAX_NUMBERED_ITEM_CHARS = 44
MAX_FALLBACK_CHARS = 96
NUMBERED_ITEM_RE = re.compile(r"^\s*\d+\.\s+(.+?)\s*$")
SOURCE_RELEASE_REFERENCE = "源码仓库同版本 Release。"


def shorten_text(value: str, limit: int) -> str:
    compact = " ".join(value.split())
    if len(compact) <= limit:
        return compact
    return compact[: max(0, limit - 1)].rstrip() + "…"


def fit_utf8(value: str, limit: int) -> str:
    encoded = value.encode("utf-8")
    if len(encoded) <= limit:
        return value
    suffix = "…"
    suffix_bytes = len(suffix.encode("utf-8"))
    payload = encoded[: max(0, limit - suffix_bytes)]
    while payload:
        try:
            return payload.decode("utf-8").rstrip() + suffix
        except UnicodeDecodeError:
            payload = payload[:-1]
    return suffix if suffix_bytes <= limit else ""


def release_details_footer(omitted: int) -> str:
    if omitted > 0:
        return f"其余 {omitted} 项见{SOURCE_RELEASE_REFERENCE}"
    return f"完整说明见{SOURCE_RELEASE_REFERENCE}"


def compact_ota_notes(version: str, release_notes: str) -> str:
    text = release_notes.strip()
    if text.startswith(f"{version}：") and (
        SOURCE_RELEASE_REFERENCE in text
        or "完整说明见同版本 Release。" in text
    ):
        return fit_utf8(text, MAX_OTA_NOTES_BYTES)

    items = [
        match.group(1)
        for line in text.splitlines()
        if (match := NUMBERED_ITEM_RE.match(line)) is not None
    ]
    if items:
        header = f"{version}：共 {len(items)} 项更新。"
        selected: list[str] = []
        for index, item in enumerate(items, start=1):
            candidate = [
                *selected,
                f"{index}. {shorten_text(item, MAX_NUMBERED_ITEM_CHARS)}",
            ]
            omitted = len(items) - len(candidate)
            footer = release_details_footer(omitted)
            result = "\n".join((header, *candidate, footer))
            if len(result.encode("utf-8")) > MAX_OTA_NOTES_BYTES:
                break
            selected = candidate

        omitted = len(items) - len(selected)
        footer = release_details_footer(omitted)
        return fit_utf8(
            "\n".join((header, *selected, footer)),
            MAX_OTA_NOTES_BYTES,
        )

    detail = text
    header_match = re.match(rf"^`?{re.escape(version)}`?：\s*", detail)
    if header_match:
        detail = detail[header_match.end():]
    detail = shorten_text(detail, MAX_FALLBACK_CHARS)
    if not detail:
        detail = "包含本版本功能修复、稳定性与资源优化。"
    return fit_utf8(
        "\n".join(
            (
                f"{version}：共 1 项更新。",
                f"1. {detail}",
                release_details_footer(0),
            )
        ),
        MAX_OTA_NOTES_BYTES,
    )


def write_atomic(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            delete=False,
        ) as handle:
            temporary_path = Path(handle.name)
            handle.write(text)
            handle.write("\n")
        temporary_path.replace(path)
    except Exception:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
        raise


def main() -> int:
    if len(sys.argv) not in (2, 4) or re.fullmatch(r"v\d+\.\d+\.\d+", sys.argv[1]) is None:
        print(
            "usage: ota_release_notes.py vX.Y.Z [--output path] < release-notes.txt",
            file=sys.stderr,
        )
        return 2
    if len(sys.argv) == 4 and sys.argv[2] != "--output":
        print(
            "usage: ota_release_notes.py vX.Y.Z [--output path] < release-notes.txt",
            file=sys.stderr,
        )
        return 2
    compact = compact_ota_notes(sys.argv[1], sys.stdin.read())
    if len(sys.argv) == 4:
        write_atomic(Path(sys.argv[3]), compact)
    else:
        print(compact)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
