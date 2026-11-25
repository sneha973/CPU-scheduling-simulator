import json, subprocess
from pathlib import Path
from django.conf import settings
import sys
ENGINE_NAME = 'scheduler_engine.exe' if sys.platform == 'win32' else 'scheduler_engine'
ROOT = Path(settings.BASE_DIR)
ENGINE = ROOT / 'build' / ENGINE_NAME
def run_engine(workload, quantum):
    if not ENGINE.is_file():
        from .reference_engine import simulate
        return simulate(workload, quantum)
    try: proc = subprocess.run([str(ENGINE)], input=json.dumps({'processes':workload,'quantum':quantum}), capture_output=True, text=True, timeout=5, check=False, cwd=ROOT)
    except subprocess.TimeoutExpired as exc: raise RuntimeError('Scheduler engine timed out.') from exc
    if proc.returncode: raise ValueError(proc.stderr.strip() or 'Scheduler engine rejected the workload.')
    try: return json.loads(proc.stdout)['results']
    except (ValueError, KeyError) as exc: raise RuntimeError('Scheduler engine returned invalid JSON.') from exc
