# PlatformIO pre-build: stamp src/version.h with v1-<short git hash>.
import subprocess, pathlib
Import("env")  # type: ignore  # noqa
try:
    h = subprocess.check_output(["git","rev-parse","--short","HEAD"],
                                cwd=env["PROJECT_DIR"]).decode().strip()
except Exception:
    h = "nogit"
v = f"v1-{h}"
out = pathlib.Path(env["PROJECT_DIR"])/"src"/"version.h"
cur = out.read_text() if out.exists() else ""
new = f'#pragma once\n#define STRUMENTO_VERSION "{v}"\n'
if cur != new:
    out.write_text(new)
print(f"[version] {v}")
