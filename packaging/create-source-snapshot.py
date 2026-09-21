#!/usr/bin/env python3
"""Create a deterministic Desktop working-tree source archive without build output.

Include tracked and non-ignored new files, honoring intentional working-tree
deletions. No commit, checkout, cleaning, dependency download or publication.
The output must be outside the source tree and must not already exist.
"""
from __future__ import annotations

import argparse
import gzip
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import tarfile


def source_files(root: Path) -> list[Path]:
    root = root.resolve(strict=True)
    top = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "--show-toplevel"], text=True
    ).strip()
    if Path(top).resolve() != root:
        raise ValueError("Source must be the root of its own Git working tree")
    names = subprocess.check_output([
        "git", "-C", str(root), "ls-files", "--cached", "--others",
        "--exclude-standard", "-z",
    ]).split(b"\0")
    result = []
    for raw in sorted(set(names)):
        if not raw:
            continue
        relative = Path(os.fsdecode(raw))
        parts = relative.parts
        if (relative.is_absolute() or ".." in parts
                or any(part in {".git", ".svn", ".hg", "__pycache__"} for part in parts)
                or any(part == "build" or part.startswith("build-") for part in parts)
                or parts[0] in {"dist", "artifacts", "stage", "pkg"}
                or any(parts[i:i + 3] in {("packaging", "arch", "src"),
                                         ("packaging", "arch", "pkg")}
                       for i in range(max(0, len(parts) - 2)))
                or ".pkg.tar." in relative.name
                or relative.name.endswith((".tar.gz", ".tar.zst", ".pyc"))):
            raise ValueError(f"Generated or unsafe file is tracked/not ignored: {relative}")
        path = root / relative
        try:
            mode = path.lstat().st_mode
        except FileNotFoundError:
            continue  # A tracked working-tree deletion must not be resurrected.
        if not (stat.S_ISREG(mode) or stat.S_ISLNK(mode)):
            raise ValueError(f"Unsupported source entry (including submodules): {relative}")
        # Never package an external link or read a file through an escaping parent.
        if not path.resolve().is_relative_to(root):
            raise ValueError(f"Source path escapes the working tree: {relative}")
        if stat.S_ISLNK(mode) and Path(os.readlink(path)).is_absolute():
            raise ValueError(f"Source symlink must be relative: {relative}")
        result.append(relative)
    for required in (Path("CMakeLists.txt"), Path("LICENSE")):
        if required not in result:
            raise ValueError(f"Missing required source file: {required}")
    return result


def create_snapshot(root: Path, output: Path, prefix: str) -> int:
    root = root.resolve(strict=True)
    output = output.absolute()
    if output.resolve().is_relative_to(root):
        raise ValueError("Output must be outside the source tree")
    if (not prefix or PurePosixPath(prefix).name != prefix
            or prefix in {".", ".."} or "\\" in prefix):
        raise ValueError("Archive prefix must be one safe directory name")
    files = source_files(root)
    with output.open("xb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as archive:
                for relative in files:
                    path = root / relative
                    info = archive.gettarinfo(str(path), arcname=f"{prefix}/{relative.as_posix()}")
                    info.uid = info.gid = info.mtime = 0
                    info.uname = info.gname = ""
                    info.mode = 0o755 if info.mode & 0o111 else 0o644
                    if info.isfile():
                        with path.open("rb") as stream:
                            archive.addfile(info, stream)
                    else:
                        archive.addfile(info)
    return len(files)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--prefix", default="aero7-desktop-0.2.0")
    args = parser.parse_args()
    count = create_snapshot(args.source, args.output, args.prefix)
    print(f"Created {args.output}: {count} source files; no working-tree changes")


if __name__ == "__main__":
    main()
