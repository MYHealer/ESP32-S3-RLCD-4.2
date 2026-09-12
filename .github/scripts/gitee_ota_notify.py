#!/usr/bin/env python3
# 通知 Gitee OTA 仓库拉取指定源码版本，不在 GitHub 运行器上传固件。
"""Notify the Gitee OTA repository without uploading firmware from GitHub."""

from __future__ import annotations

import argparse
import base64
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Optional

try:
    from github_firmware_artifacts import normalize_artifact_metadata
except ModuleNotFoundError:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from github_firmware_artifacts import normalize_artifact_metadata


VERSION_RE = re.compile(r"^v\d+\.\d+\.\d+$")
GITEE_API_BASE = "https://gitee.com/api/v5"


def parse_repository(value: str) -> tuple[str, str]:
    parts = value.strip().split("/")
    if len(parts) != 2 or not all(parts):
        raise ValueError(f"invalid repository: {value}")
    return parts[0], parts[1]


def read_json_object(path: Path, label: str) -> dict[str, object]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"unable to read {label}: {exc}") from exc
    if not isinstance(value, dict):
        raise ValueError(f"{label} root must be an object")
    return value


def release_snapshot(
    version: str,
    latest: dict[str, object],
    versions: dict[str, object],
) -> dict[str, object]:
    if latest.get("version") != version or versions.get("latest") != version:
        raise ValueError("source manifests do not publish the requested version")
    items = versions.get("items")
    if not isinstance(items, list):
        raise ValueError("source versions manifest has no items list")
    matches = [
        item
        for item in items
        if isinstance(item, dict) and item.get("version") == version
    ]
    if len(matches) != 1:
        raise ValueError("source versions manifest must contain one requested version")
    item = matches[0]
    app = normalize_artifact_metadata(item.get("app"), "source app")
    merged = normalize_artifact_metadata(item.get("merged"), "source merged")
    if app != normalize_artifact_metadata(latest, "source latest app"):
        raise ValueError("source latest and versions app metadata disagree")
    return {
        "notes": str(item.get("notes", latest.get("notes", ""))).strip(),
        "app": app,
        "merged": merged,
    }


def request(
    method: str,
    url: str,
    token: str,
    fields: Optional[dict[str, str]] = None,
    *,
    allow_missing: bool = False,
) -> Optional[dict[str, object]]:
    values = {"access_token": token}
    if fields:
        values.update(fields)
    encoded = urllib.parse.urlencode(values).encode("utf-8")
    if method == "GET":
        url = f"{url}?{encoded.decode('ascii')}"
        data = None
    else:
        data = encoded
    api_request = urllib.request.Request(
        url,
        data=data,
        method=method,
        headers={
            "Accept": "application/json",
            "Content-Type": "application/x-www-form-urlencoded",
            "User-Agent": "weather-clock-gitee-ota-notifier",
        },
    )
    try:
        with urllib.request.urlopen(api_request, timeout=60) as response:
            payload = response.read()
    except urllib.error.HTTPError as exc:
        if allow_missing and exc.code == 404:
            return None
        detail = exc.read().decode("utf-8", errors="replace")[:300]
        raise RuntimeError(f"Gitee notification failed: HTTP {exc.code}: {detail}") from exc
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"Gitee notification failed: {exc}") from exc
    if not payload:
        return {}
    value = json.loads(payload.decode("utf-8"))
    if not isinstance(value, dict):
        raise RuntimeError("Gitee notification response must be an object")
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target-repository", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-repository", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--source-run-id", required=True)
    parser.add_argument("--latest", required=True, type=Path)
    parser.add_argument("--versions", required=True, type=Path)
    parser.add_argument("--branch", default="sync-request")
    parser.add_argument("--path", default="sync/request.json")
    args = parser.parse_args()

    if not VERSION_RE.fullmatch(args.version):
        raise ValueError(f"invalid version: {args.version}")
    owner, repo = parse_repository(args.target_repository)
    parse_repository(args.source_repository)
    token = os.environ.get("GITEE_OTA_TOKEN", "").strip()
    if not token:
        raise ValueError("GITEE_OTA_TOKEN is required")

    quoted_path = "/".join(urllib.parse.quote(part, safe="") for part in args.path.split("/"))
    endpoint = (
        f"{GITEE_API_BASE}/repos/{urllib.parse.quote(owner, safe='')}/"
        f"{urllib.parse.quote(repo, safe='')}/contents/{quoted_path}"
    )
    current = request(
        "GET",
        endpoint,
        token,
        {"ref": args.branch},
        allow_missing=True,
    )
    payload = {
        "version": args.version,
        "source_repository": args.source_repository,
        "source_sha": args.source_sha,
        "source_run_id": args.source_run_id,
        "release": release_snapshot(
            args.version,
            read_json_object(args.latest, "source latest manifest"),
            read_json_object(args.versions, "source versions manifest"),
        ),
    }
    fields = {
        "content": base64.b64encode(
            (json.dumps(payload, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
        ).decode("ascii"),
        "message": f"Request {args.version} OTA mirror",
        "branch": args.branch,
    }
    method = "POST"
    if current is not None:
        sha = current.get("sha")
        if not isinstance(sha, str) or not sha:
            raise RuntimeError("Gitee request file has no SHA")
        fields["sha"] = sha
        method = "PUT"
    request(method, endpoint, token, fields)
    print(args.version)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
