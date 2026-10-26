"""
Thin Python wrapper around the C++ engine - this is what the Django layer calls.

    from engine_client import EngineClient
    engine = EngineClient("/path/to/build/schedsim")
    result = engine.simulate({"processes": [...], "time_quantum": 2})

The engine is invoked as a subprocess; JSON goes in on stdin, JSON comes out on stdout.
"""
import json
import subprocess


class EngineError(Exception):
    def __init__(self, code, message):
        super().__init__(f"[{code}] {message}")
        self.code = code
        self.message = message


class EngineClient:
    def __init__(self, binary="./build/schedsim", timeout=30):
        self.binary = binary
        self.timeout = timeout

    def _run(self, args, stdin_text=None):
        proc = subprocess.run(
            [self.binary, *args],
            input=stdin_text,
            capture_output=True,
            text=True,
            timeout=self.timeout,
        )
        out = proc.stdout.strip()
        if proc.returncode != 0:
            try:                       # errors are JSON when --out-format json (the default)
                err = json.loads(out)
                raise EngineError(err.get("code", "ENGINE_ERROR"), err.get("message", out))
            except json.JSONDecodeError:
                raise EngineError("ENGINE_CRASH", proc.stderr.strip() or out or "engine failed")
        return out

    # --- the three things Django needs -------------------------------------------------
    def simulate(self, workload: dict, algorithms=None, objective=None) -> dict:
        """workload = {"processes": [{"pid","arrival","burst","priority"}], "time_quantum": 2}"""
        args = ["simulate", "--input", "-", "--in-format", "json"]
        if algorithms:
            args += ["--algos", ",".join(algorithms)]
        if objective:
            args += ["--objective", objective]
        return json.loads(self._run(args, json.dumps(workload)))

    def features(self, workload: dict) -> dict:
        """Feature vector for the ML model (same order as training columns)."""
        return json.loads(self._run(["features", "--input", "-", "--in-format", "json"], json.dumps(workload)))

    def convert(self, text: str, in_format="auto", out_format="json", view="summary") -> str:
        """Parse an uploaded csv/txt/xml/json file and return the result in another format."""
        return self._run(["simulate", "--input", "-", "--in-format", in_format,
                          "--out-format", out_format, "--view", view], text)
