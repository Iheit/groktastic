"""
GROKTASTIC - Desktop Assets 1/2/3 -> GLB converter
Blender 5.1+

Finds these folders on the Windows Desktop:
    Assets 1
    Assets 2
    Assets 3

Every .blend file is exported to a .glb beside the original. The original
.blend is deleted ONLY after Blender reports a successful GLB export.

The work is performed by separate background Blender processes, one asset at
a time. This keeps memory usage low and avoids trying to load the whole asset
library into the interactive Blender session.

IMPORTANT:
    This intentionally removes the .blend files after successful conversion.
    If you want to keep source files, set DELETE_SOURCE_AFTER_SUCCESS = False.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

# ============================================================
# CONFIG
# ============================================================

DESKTOP = Path.home() / "Desktop"
ASSET_FOLDERS = [DESKTOP / "Assets 1", DESKTOP / "Assets 2", DESKTOP / "Assets 3"]

DELETE_SOURCE_AFTER_SUCCESS = True
OVERWRITE_EXISTING_GLB = True

# The worker receives these settings through environment variables.
WORKER_ENV_PREFIX = "GROKTASTIC_GLB_"


WORKER_SCRIPT = r'''"""Temporary worker used by convert_desktop_assets_to_glb.py."""
import bpy
import os
import sys
import traceback

source = os.environ["GROKTASTIC_GLB_SOURCE"]
output = os.environ["GROKTASTIC_GLB_OUTPUT"]

try:
    # Load the source .blend in this background Blender process.
    result = bpy.ops.wm.open_mainfile(filepath=source, load_ui=False)
    if result != {'FINISHED'}:
        raise RuntimeError("Blender could not open the source .blend")

    # Export a single binary glTF file.
    result = bpy.ops.export_scene.gltf(
        filepath=output,
        export_format='GLB',
        use_selection=False,
        export_apply=True,
        export_materials='EXPORT',
        export_cameras=False,
        export_lights=False,
    )

    if result != {'FINISHED'}:
        raise RuntimeError("Blender's glTF exporter did not finish")

    if not os.path.isfile(output) or os.path.getsize(output) < 64:
        raise RuntimeError("GLB was not created correctly")

    print("GROKTASTIC_EXPORT_SUCCESS")
    print(output)
    sys.exit(0)

except Exception as exc:
    print("GROKTASTIC_EXPORT_FAILED")
    print(type(exc).__name__ + ": " + str(exc))
    traceback.print_exc()
    sys.exit(2)
'''


def find_blend_files():
    files = []
    missing = []

    for folder in ASSET_FOLDERS:
        if not folder.is_dir():
            missing.append(folder)
            continue
        files.extend(sorted(folder.rglob("*.blend"), key=lambda p: str(p).lower()))

    return files, missing


def make_worker_script() -> Path:
    fd, name = tempfile.mkstemp(prefix="groktastic_glb_worker_", suffix=".py")
    path = Path(name)
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        f.write(WORKER_SCRIPT)
    return path


def blender_executable() -> str:
    # In Blender's Python, sys.executable normally points to the Blender binary.
    exe = Path(sys.executable)
    if exe.name.lower().startswith("blender"):
        return str(exe)

    # Fallback for unusual installations where sys.executable is not Blender.
    candidates = [
        Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / "Blender Foundation" / "Blender" / "blender.exe",
        Path(os.environ.get("PROGRAMFILES", "C:/Program Files")) / "Blender Foundation" / "Blender 5.1" / "blender.exe",
        Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "Blender Foundation" / "Blender" / "blender.exe",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return str(candidate)

    raise RuntimeError(
        "Could not locate blender.exe. Run this script from Blender 5.1.2's Scripting workspace."
    )


def convert_one(blend_path: Path, worker: Path, blender: str, index: int, total: int):
    glb_path = blend_path.with_suffix(".glb")

    print(f"\n[{index}/{total}] {blend_path}")

    if glb_path.exists() and not OVERWRITE_EXISTING_GLB:
        print("  SKIP: GLB already exists")
        return "skipped"

    # Remove a stale destination before export so a failed export cannot be
    # mistaken for a newly successful conversion.
    if glb_path.exists():
        try:
            glb_path.unlink()
        except OSError as exc:
            print(f"  ERROR: cannot remove existing GLB: {exc}")
            return "failed"

    env = os.environ.copy()
    env["GROKTASTIC_GLB_SOURCE"] = str(blend_path)
    env["GROKTASTIC_GLB_OUTPUT"] = str(glb_path)

    command = [
        blender,
        "--background",
        "--factory-startup",
        "--python",
        str(worker),
    ]

    try:
        completed = subprocess.run(
            command,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
    except Exception as exc:
        print(f"  ERROR: could not start Blender: {exc}")
        return "failed"

    success_marker = "GROKTASTIC_EXPORT_SUCCESS" in completed.stdout
    valid_glb = glb_path.is_file() and glb_path.stat().st_size >= 64

    if success_marker and valid_glb:
        size_mb = glb_path.stat().st_size / (1024 * 1024)
        print(f"  OK: {glb_path.name} ({size_mb:.2f} MB)")

        if DELETE_SOURCE_AFTER_SUCCESS:
            try:
                blend_path.unlink()
                print("  Deleted source .blend after successful export")
            except OSError as exc:
                print(f"  WARNING: GLB succeeded but source could not be deleted: {exc}")

        return "converted"

    print("  FAILED: export did not complete successfully")
    # Print the useful tail instead of flooding the Blender console.
    lines = completed.stdout.splitlines()
    for line in lines[-25:]:
        print("    " + line)
    return "failed"


def main():
    print("=" * 64)
    print("GROKTASTIC DESKTOP ASSET CONVERTER")
    print("Blender 5.1+")
    print("=" * 64)
    print(f"Desktop: {DESKTOP}")
    print("Folders:")
    for folder in ASSET_FOLDERS:
        print(f"  - {folder}")

    blend_files, missing = find_blend_files()

    if missing:
        print("\nMissing folders:")
        for folder in missing:
            print(f"  - {folder}")

    if not blend_files:
        print("\nNo .blend files were found in Assets 1, Assets 2, or Assets 3.")
        print("Nothing was changed.")
        return

    print(f"\nFound {len(blend_files)} .blend file(s).")
    print(f"Delete source after successful export: {DELETE_SOURCE_AFTER_SUCCESS}")
    print(f"Overwrite existing GLB: {OVERWRITE_EXISTING_GLB}")

    blender = blender_executable()
    print(f"Worker Blender: {blender}")

    worker = make_worker_script()
    counts = {"converted": 0, "skipped": 0, "failed": 0}

    try:
        for index, blend_path in enumerate(blend_files, 1):
            status = convert_one(blend_path, worker, blender, index, len(blend_files))
            counts[status] += 1
    finally:
        try:
            worker.unlink()
        except OSError:
            pass

    print("\n" + "=" * 64)
    print("CONVERSION COMPLETE")
    print(f"Converted: {counts['converted']}")
    print(f"Skipped:   {counts['skipped']}")
    print(f"Failed:    {counts['failed']}")
    print("=" * 64)

    if counts["failed"]:
        print("Some files failed. Their .blend sources were NOT deleted.")
    else:
        print("All discovered .blend files converted successfully.")


if __name__ == "__main__":
    main()
