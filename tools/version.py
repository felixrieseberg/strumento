# PlatformIO pre-build: stamp src/version.h with YYMMDD-<short git hash>.
# Uses the *commit* date so the version is deterministic across rebuilds.
import subprocess, pathlib
Import("env")  # type: ignore  # noqa
try:
    sha  = subprocess.check_output(["git","rev-parse","--short=7","HEAD"],
                                   cwd=env["PROJECT_DIR"]).decode().strip()
    date = subprocess.check_output(["git","log","-1","--format=%cd",
                                    "--date=format:%y%m%d"],
                                   cwd=env["PROJECT_DIR"]).decode().strip()
    v = f"{date}-{sha}"
except Exception:
    v = "000000-nogit"
out = pathlib.Path(env["PROJECT_DIR"])/"src"/"version.h"
cur = out.read_text() if out.exists() else ""
new = f'#pragma once\n#define STRUMENTO_VERSION "{v}"\n'
if cur != new:
    out.write_text(new)
print(f"[version] {v}")
