#!/usr/bin/env python3
# 统一 GitHub 源码构建与 OTA 镜像使用的固件及清单契约。
"""Define firmware and manifest contracts shared by GitHub automation."""

from __future__ import annotations

import json
import os
import re
import tempfile
from pathlib import Path
from typing import NamedTuple


FIELD_LATEST = "latest"
FIELD_ITEMS = "items"
FIELD_VERSION = "version"
FIELD_NOTES = "notes"
FIELD_URL = "url"
FIELD_SHA256 = "sha256"
FIELD_SIZE = "size"
LATEST_MAX_BYTES = 1800
VERSIONS_KEEP = 10
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
VERSION_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)$")


class FirmwareArtifactSpec(NamedTuple):
    manifest_key: str
    filename_suffix: str
    release_description: str


FIRMWARE_ARTIFACTS = (
    FirmwareArtifactSpec("app", ".bin", "设备 OTA 升级固件。"),
    FirmwareArtifactSpec("merged", "_merged.bin", "包含分区表的完整刷写固件。"),
)


def version_key(value: str) -> tuple[int, int, int]:
    match = VERSION_RE.fullmatch(value)
    if not match:
        raise ValueError(f"invalid version: {value}")
    return tuple(int(part) for part in match.groups())


def encode_json(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def atomic_write(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", dir=path.parent
    )
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(content)
        os.replace(temporary_name, path)
    except Exception:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise


def normalize_artifact_metadata(
    value: object,
    label: str,
) -> dict[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} metadata must be an object")
    sha256 = str(value.get(FIELD_SHA256, "")).lower()
    size = value.get(FIELD_SIZE)
    if not SHA256_RE.fullmatch(sha256):
        raise ValueError(f"{label} has an invalid SHA256")
    if isinstance(size, bool) or not isinstance(size, int) or size <= 0:
        raise ValueError(f"{label} has an invalid size")
    return {FIELD_SHA256: sha256, FIELD_SIZE: size}


def firmware_artifact_name(version: str, artifact: FirmwareArtifactSpec) -> str:
    return f"weather_clock_{version}{artifact.filename_suffix}"
