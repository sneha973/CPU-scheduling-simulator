"""
End-to-end demo of what the Django view will do for one user request:

    1. engine.simulate()  -> metrics + Gantt for every algorithm   (ground truth)
    2. engine.features()  -> feature vector -> ML model            (prediction)

    python python_examples/recommend.py examples/workload.json model.joblib
"""
import json
import sys

import joblib
import pandas as pd

from engine_client import EngineClient

workload = json.load(open(sys.argv[1] if len(sys.argv) > 1 else "examples/workload.json"))
bundle = joblib.load(sys.argv[2] if len(sys.argv) > 2 else "model.joblib")

engine = EngineClient("./build/schedsim")
sim = engine.simulate(workload)
feats = engine.features(workload)

# Reorder by the names stored at training time - safe even if the engine ever reorders columns.
row = pd.DataFrame([feats["features"]])[bundle["feature_names"]]
proba = bundle["model"].predict_proba(row)[0]
ranked = sorted(zip(bundle["model"].classes_, proba), key=lambda t: -t[1])

print("Engine (exact simulation) says best :", sim["best"]["algorithm"])
print("ML model recommends                 :", ranked[0][0], f"({ranked[0][1]:.0%} confidence)")
print("All probabilities                   :", {k: round(float(v), 3) for k, v in ranked})
