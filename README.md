# Web-Based CPU Scheduling Simulator

An educational, local web application that runs one workload through FCFS, non-preemptive SJF, Round Robin, and non-preemptive Priority Scheduling. The C++17 engine returns JSON timelines and scheduling metrics; Django saves runs in SQLite and serves the dashboard and JSON endpoints.

## Implemented scope and assumptions

- One CPU; nonnegative integer arrivals, positive integer burst times and RR quantum.
- Lower numeric priority means higher priority. Ties resolve by arrival time, then process ID. SJF ties use burst time, then arrival and ID. FCFS uses arrival and ID.
- Context-switch overhead is zero. A switch is counted between consecutive CPU allocations to different processes; idle gaps do not add a switch.
- The recommendation objective is equal-weighted normalized average waiting time and response time. The selected algorithm and all workload feature values are returned. A deterministic decision tree can be generated from simulator-labelled synthetic data; before training, the app falls back to the direct score winner.
- The source synopsis lists Django REST Framework, pandas/NumPy and scikit-learn. The current sandbox has Django and pandas/NumPy but lacks DRF and scikit-learn, and cannot reach package indexes. The JSON API is implemented with Django views; `requirements.txt` records the intended dependencies for installation where network/package access is available. The bundled training script uses a small native CART implementation so dataset generation and measured evaluation do not depend on scikit-learn.
- The PPT mentions preemptive SRTF and live Linux process traces. They are not included because the synopsis objectives define non-preemptive SJF/Priority and explicitly list live traces as a possible extension. No claims are made about host scheduler behavior.

## Requirements

- Windows with Python 3.10+ and Django 5.2+ (Django 6 works on Python 3.12+).
- MinGW-w64 `g++` on `PATH`, or adjust `scripts/build_engine.ps1` to your C++17 compiler.
- Optional libraries in `requirements.txt` for the documented stack. This implementation's core does not require external Python packages beyond Django.

## Setup and run

From PowerShell in the project directory:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

Build the engine and initialize the local database:

```powershell
.\scripts\build_engine.ps1
python manage.py migrate
python manage.py runserver
```

Open http://127.0.0.1:8000/. Choose **Load sample**, adjust the process rows or RR quantum, and select **Run all algorithms**. Runs are stored in `db.sqlite3`.

For Python environments with no network, if Django is already present, skip dependency installation and use the same build, migrate, and run commands. The dashboard still uses SQLite and the C++ executable.

## API

### `POST /api/simulate/`

Request JSON:

```json
{"quantum":2,"processes":[{"id":"P1","arrival":0,"burst":5,"priority":2},{"id":"P2","arrival":1,"burst":3,"priority":1}]}
```

Returns run ID/time, normalized input, each algorithm's `timeline`, `process_metrics`, and `aggregate` metrics, and a `recommendation`. Timeline entries have process ID and start/end time. Process metrics include completion, turnaround, waiting, and response times. Aggregate metrics include averages, throughput (completed processes per elapsed simulation unit), context-switch count, and CPU utilization percentage. Invalid payloads return HTTP 400; an unavailable or timed-out engine returns HTTP 503.

### `GET /api/history/`

Returns the 25 latest saved run IDs, timestamps, process counts, and recommendation labels. Detailed run data is persisted in SQLite.

## Synthetic dataset and model

With the C++ engine built, run:

```powershell
python scripts/train_model.py
```

This uses seed `20261023`, generates 2,400 workloads, labels each by running all four schedulers and choosing the minimum equal-weighted normalized average waiting/response score, then performs an 80/20 stratified split. It uses the C++ executable when available and records when it must use the Python reference fallback. Outputs are `data/synthetic_workloads.csv`, `data/model.json`, and `data/evaluation.json`. The report includes held-out accuracy, class-wise precision/recall, and a confusion matrix. The 80% accuracy is a target, not a guarantee; inspect `target_met` in the report. Once `data/model.json` exists, the Django endpoint uses its tree prediction and reports the measured accuracy. With the documented objective, the non-preemptive SJF often dominates on average waiting/response, so the generator may produce no Priority winners; zero support for a class is reported, not concealed.

## Tests

```powershell
python manage.py test simulator
```

Tests use the actual compiled C++ process and Django's test client. They cover hand-checked FCFS results, all algorithm endpoints, invalid inputs, an idle interval, and saved history.

## Structure

```text
engine/main.cpp             C++17 scheduler and JSON contract
simulator/engine.py         Safe timed subprocess bridge
simulator/views.py          Input validation, simulation and history API
simulator/models.py         SQLite run persistence
simulator/recommend.py      Features and recommendation objective
simulator/classifier.py     Trained-tree loading/inference
scripts/train_model.py      Reproducible generation, labeling, training, metrics
templates/index.html        Browser UI
static/app.js               Workload form and result rendering
static/app.css              Responsive dashboard styling
```
