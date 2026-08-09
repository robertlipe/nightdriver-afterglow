Import("env")
import subprocess
import sys

print("[Shared-Libs] Running shared library setup...")
try:
    subprocess.check_call([sys.executable, "tools/setup_shared_libs_cli.py"])
except subprocess.CalledProcessError as e:
    print(f"[Shared-Libs] Error: Setup script failed with code {e.returncode}")
    sys.exit(e.returncode)
