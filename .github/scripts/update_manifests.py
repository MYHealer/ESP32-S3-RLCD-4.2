#!/usr/bin/env python3
"""根据 GitHub Actions 的实际构建产物生成 OTA latest/versions 清单。"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from urllib.parse import urlparse

try:
    from ota_release_notes import compact_ota_notes
except ModuleNotFoundError:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from ota_release_notes import compact_ota_notes

try:
    from github_firmware_artifacts import (
        FIELD_ITEMS,
        FIELD_LATEST,
        FIELD_NOTES,
        FIELD_SHA256,
        FIELD_SIZE,
        FIELD_URL,
        FIELD_VERSION,
        FIRMWARE_ARTIFACTS,
        LATEST_MAX_BYTES,
        VERSIONS_KEEP,
        atomic_write,
        encode_json,
        firmware_artifact_name,
        version_key,
    )
except ModuleNotFoundError:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from github_firmware_artifacts import (
        FIELD_ITEMS,
        FIELD_LATEST,
        FIELD_NOTES,
        FIELD_SHA256,
        FIELD_SIZE,
        FIELD_URL,
        FIELD_VERSION,
        FIRMWARE_ARTIFACTS,
        LATEST_MAX_BYTES,
        VERSIONS_KEEP,
        atomic_write,
        encode_json,
        firmware_artifact_name,
        version_key,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--version", required=True)
    parser.add_argument("--app", required=True, type=Path)
    parser.add_argument("--merged", required=True, type=Path)
    parser.add_argument("--artifact-base", required=True)
    parser.add_argument("--notes-file", required=True, type=Path)
    parser.add_argument("--latest", required=True, type=Path)
    parser.add_argument("--versions", required=True, type=Path)
    return parser.parse_args()


def require_version(value: str) -> tuple[int, int, int]:
    return version_key(value)


def normalize_public_base(value: str) -> str:
    normalized = value.strip().rstrip("/")
    parsed = urlparse(normalized)
    if parsed.scheme != "https" or not parsed.netloc or parsed.query or parsed.fragment:
        raise ValueError("public base must be an HTTPS origin or path without query/fragment")
    return normalized


def load_release_notes(path: Path, version: str) -> str:
    try:
        source = path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeDecodeError) as exc:
        raise ValueError(f"unable to read OTA release notes: {exc}") from exc
    if not source.startswith(f"{version}："):
        raise ValueError("OTA release notes version does not match the build version")
    notes = compact_ota_notes(version, source)
    if not notes.startswith(f"{version}："):
        raise ValueError("compacted OTA release notes lost their version header")
    return notes


def file_metadata(path: Path) -> tuple[str, int]:
    if not path.is_file() or path.stat().st_size <= 0:
        raise ValueError(f"firmware file is missing or empty: {path}")
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest(), path.stat().st_size


def fit_latest_notes(manifest: dict[str, object], notes: str) -> bytes:
    manifest[FIELD_NOTES] = notes
    encoded = encode_json(manifest)
    if len(encoded) <= LATEST_MAX_BYTES:
        return encoded

    suffix = "…"
    low = 0
    high = len(notes)
    best = ""
    while low <= high:
        middle = (low + high) // 2
        candidate = notes[:middle].rstrip() + suffix
        manifest[FIELD_NOTES] = candidate
        encoded = encode_json(manifest)
        if len(encoded) <= LATEST_MAX_BYTES:
            best = candidate
            low = middle + 1
        else:
            high = middle - 1
    manifest[FIELD_NOTES] = best
    encoded = encode_json(manifest)
    if len(encoded) > LATEST_MAX_BYTES:
        raise ValueError("latest manifest metadata exceeds size limit")
    return encoded


def load_versions(path: Path) -> list[dict[str, object]]:
    if not path.is_file():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"unable to read existing versions manifest: {exc}") from exc
    items = data.get(FIELD_ITEMS) if isinstance(data, dict) else None
    if not isinstance(items, list):
        raise ValueError("existing versions manifest does not contain an items list")
    return [item for item in items if isinstance(item, dict)]


def main() -> int:
    args = parse_args()
    require_version(args.version)
    artifact_base = normalize_public_base(args.artifact_base)
    notes = load_release_notes(args.notes_file, args.version)
    artifact_paths = {"app": args.app, "merged": args.merged}
    artifact_metadata = {
        artifact.manifest_key: file_metadata(artifact_paths[artifact.manifest_key])
        for artifact in FIRMWARE_ARTIFACTS
    }
    current = {
        FIELD_VERSION: args.version,
        FIELD_NOTES: notes,
        **{
            artifact.manifest_key: {
                FIELD_URL: (
                    f"{artifact_base}/"
                    f"{firmware_artifact_name(args.version, artifact)}"
                ),
                FIELD_SHA256: artifact_metadata[artifact.manifest_key][0],
                FIELD_SIZE: artifact_metadata[artifact.manifest_key][1],
            }
            for artifact in FIRMWARE_ARTIFACTS
        },
    }

    existing = [
        item
        for item in load_versions(args.versions)
        if item.get(FIELD_VERSION) != args.version
    ]
    for item in existing:
        version = str(item.get(FIELD_VERSION, ""))
        require_version(version)
        item[FIELD_NOTES] = compact_ota_notes(
            version,
            str(item.get(FIELD_NOTES, "")),
        )
    sortable = [current, *existing]
    sortable.sort(
        key=lambda item: require_version(str(item.get(FIELD_VERSION, ""))),
        reverse=True,
    )
    versions = {
        FIELD_LATEST: args.version,
        FIELD_ITEMS: sortable[:VERSIONS_KEEP],
    }

    latest_app = current["app"]
    latest = {
        FIELD_VERSION: args.version,
        FIELD_URL: latest_app[FIELD_URL],
        FIELD_SHA256: latest_app[FIELD_SHA256],
        FIELD_SIZE: latest_app[FIELD_SIZE],
    }
    atomic_write(args.latest, fit_latest_notes(latest, notes))
    atomic_write(args.versions, encode_json(versions))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
