# schedsim - C++ CPU Scheduling Engine

The C++ half of **"Web-Based CPU Scheduling Simulator & ML-Powered Workload Predictor"**.
It has exactly two jobs:

| # | Job | Command |
|---|-----|---------|
| 1 | **Train the ML model**: generate synthetic workloads, run every algorithm on each one, label it with the winner, and export a training dataset | `schedsim generate` |
| 2 | **Run the algorithms**: parse workloads in JSON / CSV / TXT / XML, simulate FCFS / SJF / Round Robin / Priority (+ SRTF and preemptive Priority), and output results as JSON / CSV / text / XML | `schedsim simulate` |

A third helper, `schedsim features`, returns the ML feature vector of a workload so Django can feed the trained model at prediction time.

No third-party dependencies. C++17, any compiler (g++, clang, MSVC).

---

## 1. Build and test

```bash
make            # builds build/schedsim
make test       # runs the unit tests (48,000+ checks)
```
or with CMake:
```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

## 2. Project layout

```
include/schedsim/
  types.hpp       Process, Workload, Metrics, Report ... (shared data model)
  schedulers.hpp  the algorithms + input validation
  analysis.hpp    runs several algorithms, scores them, picks the best
  features.hpp    workload -> ML feature vector
  generator.hpp   synthetic workload generator + auto-labelling (ML training data)
  parsers.hpp     JSON / CSV / TXT / XML input
  writers.hpp     JSON / CSV / text / XML output
  json.hpp util.hpp
src/              implementations + main.cpp (CLI)
tests/            test_engine.cpp (hand-verified textbook workloads + invariants)
examples/         the same workload in all four input formats
python_examples/  engine_client.py (for Django), train_model.py, recommend.py
```

## 3. Command line

```
schedsim simulate  [--input FILE|-] [--in-format auto|json|csv|txt|xml]
                   [--output FILE] [--out-format json|csv|text|xml]
                   [--algos fcfs,sjf,rr,priority | all | extended]
                   [--quantum N] [--objective balanced|wt|tat|rt|pwt]
                   [--view summary|processes|gantt] [--pretty]
schedsim features  [same input options]
schedsim generate  [--count N] [--seed S] [--format csv|json|xml] [--output FILE]
                   [--min-procs N] [--max-procs N] [--balance] [--with-metrics]
                   [--algos ...] [--objective ...]
schedsim algorithms
```

* Input comes from `--input FILE` or **stdin** (`-` or omitted). `--in-format auto` detects by file extension, then by content.
* Exit code **0** = success. **1** = error; the reply is `{"status":"error","code":"...","message":"..."}` on stdout (JSON mode) or plain text on stderr (other formats).
* Error codes: `PARSE_ERROR`, `INVALID_INPUT`, `IO_ERROR`, `USAGE_ERROR`, `INTERNAL_ERROR`.

Quick tries:
```bash
./build/schedsim simulate --input examples/workload.json --out-format text
./build/schedsim simulate --input examples/workload.csv  --out-format csv --view gantt
./build/schedsim simulate --input examples/workload.xml  --out-format xml
```

## 4. Input formats (all four give identical results)

Process fields: `pid` (optional, auto-named P1, P2...), `arrival` (default 0), `burst` (**required**, integer >= 1), `priority` (default 0, **lower number = higher priority**). Common aliases are accepted (`arrival_time`, `burst_time`, `AT`, `BT`, `prio`, `id`, ...).
Settings: `time_quantum` (default 2), `algorithms`, `objective`. CLI flags override the file.

**JSON**
```json
{ "processes": [ {"pid":"P1","arrival":0,"burst":5,"priority":3} ],
  "time_quantum": 2, "algorithms": ["fcfs","sjf","rr","priority"], "objective": "balanced" }
```
(A bare top-level array of processes also works; settings may sit inside `"config"` / `"options"`.)

**CSV** - header optional, delimiter `,` `;` or tab, UTF-8 BOM ok. Settings via `# quantum=2` comments or a `quantum,2` row.
```
# quantum=2
pid,arrival,burst,priority
P1,0,5,3
```
**TXT** - whitespace separated, `quantum 2` line, `#` comments, header optional.
```
quantum 2
P1 0 5 3
```
**XML** - attributes or child elements.
```xml
<workload quantum="2">
  <process pid="P1" arrival="0" burst="5" priority="3"/>
  <process><pid>P2</pid><arrival>1</arrival><burst>3</burst></process>
</workload>
```

## 5. Output: the JSON contract with Django

`schedsim simulate` returns (abridged):
```jsonc
{
  "status": "ok", "engine_version": "1.0.0",
  "input":   { "time_quantum": 2, "objective": "balanced", "processes": [ {"pid","arrival","burst","priority"} ] },
  "features": { "num_processes": 5, "mean_burst": 2.8, ... },        // ordered, see section 7
  "feature_vector": [5, 2.8, ...],
  "results": [
    { "algorithm": "fcfs", "label": "FCFS", "score": 1.2455,
      "metrics": { "avg_waiting_time", "avg_turnaround_time", "avg_response_time",
                   "priority_weighted_waiting_time", "cpu_utilization", "throughput",
                   "max_waiting_time", "makespan", "total_burst_time", "idle_time", "context_switches" },
      "gantt":  [ {"pid":"P1","start":0,"end":5}, {"pid":"IDLE","start":5,"end":7} ],   // ready for Chart.js/Plotly
      "processes": [ {"pid","arrival","burst","priority","first_start","completion","waiting","turnaround","response"} ] }
  ],
  "best": { "algorithm": "sjf", "label": "...", "objective": "balanced", "score": 1.1412 }
}
```
`best` is the **exact, simulated** winner (ground truth). The ML model predicts this without simulating.

CSV views: `--view summary` (one row per algorithm), `processes` (one row per algorithm x process), `gantt` (one row per segment).

## 6. Algorithm semantics (write these in your report)

| id | Algorithm | Selection rule |
|----|-----------|----------------|
| `fcfs` | First Come First Served | earliest arrival |
| `sjf` | Shortest Job First, non-preemptive | smallest burst among arrived |
| `srtf` | Shortest Remaining Time First (preemptive SJF) | smallest remaining time, re-evaluated at every arrival |
| `priority` | Priority, non-preemptive | smallest priority number among arrived |
| `priority_preemptive` | Priority, preemptive | same, re-evaluated at every arrival |
| `rr` | Round Robin | FIFO queue, `time_quantum` slice |

`all` = `fcfs,sjf,rr,priority` (the four from the synopsis). `extended` adds `srtf` and `priority_preemptive`.

Conventions: ties are broken by arrival time, then input order. In Round Robin, processes that arrive during a slice are queued **before** the preempted process. Idle CPU time appears as an `IDLE` Gantt segment. Metrics: `TAT = completion - arrival`, `WT = TAT - burst`, `RT = first_start - arrival`. `CPU utilisation = total burst / (last completion - first arrival) x 100`. `context_switches` counts changes from one process to a different one.

## 7. Training the ML model (job #1)

```bash
./build/schedsim generate --count 20000 --seed 42 --balance --output dataset.csv
python python_examples/train_model.py dataset.csv model.joblib
```
Each row = one random workload: 15 feature columns + `best_algorithm`. The label is **computed by running the simulators**, not guessed. Generation covers uniform / near-equal / bimodal / heavy-tailed / identical / convoy burst patterns, simultaneous / clustered / sparse / trickling arrivals, and random / short-first / long-first / flat priorities. Same `--seed` gives byte-identical datasets on every OS and compiler.

Features (the order is part of the contract; also returned by `schedsim features`):
`num_processes, mean_burst, burst_variance, burst_cv, burst_max_min_ratio, arrival_span, arrival_density, mean_interarrival, load_factor, time_quantum, quantum_to_burst_ratio, priority_variance, priority_burst_corr, arrival_burst_corr, priority_arrival_corr`

**What "best" means** (`--objective`): `wt`, `tat`, `rt`, `pwt` (priority-weighted waiting time: urgent processes count more) or `balanced` (default: 25% WT + 15% TAT + 25% RT + 10% worst-case WT + 25% PWT, each divided by the best any algorithm achieves). Pure waiting time would make SJF win ~70% of the time and Priority almost never, so a classifier would never recommend Priority. `balanced` rewards each algorithm for what it is actually good at. Weights are constants at the top of `score_results()` in `src/analysis.cpp`.

Measured with the included script (20,000 balanced samples, 80/20 split): **Random Forest 84.6%, Decision Tree 81.8%** (target in the synopsis: >= 80%).

## 8. Using it from Django

```python
from engine_client import EngineClient            # python_examples/engine_client.py
engine = EngineClient("/abs/path/to/build/schedsim")

result = engine.simulate({"processes": rows, "time_quantum": 3})   # Gantt + metrics for 4 algorithms
feats  = engine.features({"processes": rows, "time_quantum": 3})   # -> model.predict_proba(...)
csv_text = engine.convert(uploaded_file_text, out_format="csv")   # file upload in any format
```
`python_examples/recommend.py` shows the full simulate + predict flow. Pass `--objective` / `objective=` identical to the one used at training time.

## 9. Limits and behaviour

* Max 5,000 processes; arrival / burst / quantum <= 1,000,000; integers only. Round Robin refuses workloads needing more than 5,000,000 slices.
* 5,000 processes x 6 algorithms simulate in under a second.
* Every algorithm is tested against hand-computed textbook results, plus invariants (TAT = WT + burst, Gantt busy time = total burst, ...) on 150 random workloads, and the suite runs clean under AddressSanitizer / UBSan.
