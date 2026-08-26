Import("env")
import subprocess
import sys

print(">> Running pre-build style and include audits...")
try:
    subprocess.check_call([sys.executable, 'tools/audit_include_rules.py'])
    subprocess.check_call([sys.executable, 'tools/audit_globals_order.py'])
except subprocess.CalledProcessError:
    print(">> ERROR: Code style or include rules audit failed! Fix violations to build.", file=sys.stderr)
    sys.exit(1)
