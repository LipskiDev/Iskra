"""Compile the stages declared on an HLSL file's first line with DXC."""

import argparse
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile


ENTRY_POINTS = {
    "vs": "VSMain",
    "ps": "PSMain",
    "gs": "GSMain",
    "hs": "HSMain",
    "ds": "DSMain",
    "cs": "CSMain",
    "ms": "MSMain",
    "as": "ASMain",
    "lib": None,
}


def read_stages(source):
    with source.open(encoding="utf-8-sig") as shader:
        first_line = shader.readline().strip()
    declaration = re.fullmatch(r"//\s*@stages\s+(.+)", first_line)
    if not declaration:
        raise ValueError(f"{source}: first line must declare stages, e.g. //@stages ms ps")

    stages = declaration.group(1).split()
    unknown = sorted(set(stages) - ENTRY_POINTS.keys())
    if unknown:
        raise ValueError(f"{source}: unknown stages: {', '.join(unknown)}")
    if len(stages) != len(set(stages)):
        raise ValueError(f"{source}: duplicate stages in declaration")
    return stages


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--output-directory", required=True, type=pathlib.Path)
    parser.add_argument("--configuration", choices=("Debug", "Release", "Profile"), default="Debug")
    parser.add_argument("--dxc", default="dxc.exe", help="DXC executable name or path")
    args = parser.parse_args()

    try:
        source = args.source.resolve(strict=True)
        stages = read_stages(source)
        compiler = shutil.which(args.dxc)
        if compiler is None:
            raise ValueError("DXC was not found. Add dxc.exe to PATH or pass --dxc PATH.")

        output_directory = args.output_directory.resolve()
        output_directory.mkdir(parents=True, exist_ok=True)
        flags = ["-Zi", "-Qembed_debug", "-Od"] if args.configuration == "Debug" else ["-O3"]

        # Publish outputs only once every requested stage compiled successfully.
        with tempfile.TemporaryDirectory(prefix="shader-", dir=output_directory) as temporary:
            for stage in stages:
                output = pathlib.Path(temporary) / f"{source.stem}.{stage}.dxil"
                entry_point = ENTRY_POINTS[stage]
                entry_flags = ["-E", entry_point] if entry_point is not None else []
                print(f"Compiling {source.name}: {entry_point or 'shader library'} ({stage}_6_5)", flush=True)
                subprocess.run([
                    compiler, "-T", f"{stage}_6_5", *entry_flags,
                    "-I", str(source.parent), *flags, "-Fo", str(output), str(source),
                ], check=True)

            for stage in stages:
                name = f"{source.stem}.{stage}.dxil"
                (pathlib.Path(temporary) / name).replace(output_directory / name)
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Shader compilation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
