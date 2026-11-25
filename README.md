# SchedLab — CPU Scheduling Simulator with ML Predictor

[![Django](https://img.shields.io/badge/Django-5.2-092E20?logo=django)](https://www.djangoproject.com/)
[![C++17](https://img.shields.io/badge/Engine-C%2B%2B17-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

An interactive, responsive educational web application that simulates and analyzes four CPU scheduling algorithms: **First-Come, First-Served (FCFS)**, **Non-preemptive Shortest Job First (SJF)**, **Round Robin (RR)**, and **Non-preemptive Priority**. The system computes detailed per-process metrics, renders colored Gantt timelines, compares aggregate performance, and recommends the optimal algorithm using a trained machine-learning model.

---

## Key Features

- **4 CPU Scheduling Policies**:
  - **FCFS**: Non-preemptive, FIFO arrival ordering with tie-breaking on arrival time and process ID.
  - **SJF**: Non-preemptive shortest job first with burst time priority.
  - **Round Robin**: Preemptive time-sliced allocation with configurable quantum $q$.
  - **Priority Scheduling**: Non-preemptive priority-ordered execution (lower number = higher priority).
- **ML Algorithm Recommendation**:
  - Uses an 8-feature CART Decision Tree trained on 2,400 synthetic workloads to predict the optimal algorithm.
  - Objective: Minimize equal-weighted normalized average waiting time and response time.
  - Transparent feature extraction: burst mean, burst coefficient of variation, arrival density, priority spread, and quantum/burst ratio.
- **Modern & Responsive UI**:
  - Designed with an attractive **indigo, teal, and coral** color palette.
  - **Consistent Process Color Palette**: Each process maintains its distinct color across the workload editor, Gantt timeline, and algorithm legends.
  - Workload Presets: *Classic Balanced*, *High Burst Variance (Convoy effect)*, and *Staggered Arrivals*.
  - Aggregate performance comparison matrix with best-value highlights.
- **Persistent Run History**:
  - Simulations are saved to PostgreSQL (production) or SQLite (local).
  - **1-Click Reload**: Restore any previous simulation workload back into the editor with one click.
- **High-Performance Hybrid Engine**:
  - Fast compiled C++17 scheduling engine executed via timed subprocess.
  - Graceful fallback to pure-Python reference engine if a C++ compiler is not present.

---

## Project Architecture

```text
├── engine/
│   └── main.cpp             # Fast C++17 scheduling engine
├── simulator/
│   ├── engine.py            # Timed subprocess runner with fallback
│   ├── reference_engine.py  # Pure Python reference engine fallback
│   ├── recommend.py         # Workload feature extractor & recommender
│   ├── classifier.py        # Decision tree model inference
│   ├── models.py            # SimulationRun ORM model
│   ├── views.py             # REST API endpoints (simulate, history, health)
│   └── tests.py             # Automated unit tests
├── scheduler_site/
│   ├── settings.py          # Production & local settings (WhiteNoise, dj-database-url)
│   ├── urls.py              # URL routing
│   └── wsgi.py              # WSGI entrypoint for Gunicorn
├── static/
│   ├── app.css              # Responsive modern styles (indigo, teal, coral)
│   └── app.js               # Interactive Gantt rendering & state management
├── templates/
│   └── index.html           # Single-page dashboard application
├── data/
│   ├── model.json           # Serialized CART decision tree model
│   ├── evaluation.json      # Model accuracy and evaluation metrics
│   └── synthetic_workloads.csv # Labeled training dataset
├── build.sh                 # Render automated build script
├── render.yaml              # Render Infrastructure-as-Code Blueprint
├── requirements.txt         # Production Python dependencies
└── manage.py                # Django management script
```

---

## Local Development Setup

### Prerequisites
- Python 3.10, 3.11, or 3.12
- Optional: `g++` with C++17 support (MinGW on Windows, GCC on Linux/macOS)

### 1. Clone the repository
```bash
git clone https://github.com/sneha973/CPU-scheduling-simulator.git
cd CPU-scheduling-simulator
```

### 2. Set up virtual environment
```bash
# Linux / macOS:
python3 -m venv .venv
source .venv/bin/activate

# Windows (PowerShell):
python -m venv .venv
.\.venv\Scripts\Activate.ps1
```

### 3. Install dependencies
```bash
pip install -r requirements.txt
```

### 4. Build C++ engine (Optional, recommended)
```bash
# Linux / macOS:
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra engine/main.cpp -o build/scheduler_engine

# Windows:
mkdir build
g++ -std=c++17 -O2 engine/main.cpp -o build/scheduler_engine.exe
```
*(If omitted, the app automatically falls back to the bundled Python reference engine)*

### 5. Run migrations & start server
```bash
python manage.py migrate
python manage.py runserver
```

Open your browser at **http://127.0.0.1:8000/**.

---

## Free-Tier Cloud Deployment (Render + Neon)

This project is pre-configured for **100% free hosting** using:
1. **Render**: Free web service for running Django + Gunicorn.
2. **Neon**: Free serverless PostgreSQL database for persistent simulation history.

### Step 1: Create a Free PostgreSQL Database on Neon
1. Go to [neon.tech](https://neon.tech/) and create a free account.
2. Click **Create Project**, name it `cpu-scheduler-db`, and choose a region close to your users (e.g., *Singapore (ap-southeast-1)*).
3. Under **Connection Details**, select **Connection String** and choose **Pooled connection**.
4. Copy the connection string. It looks like:
   ```text
   postgresql://sneha_owner:password@ep-cool-snowflake-123456-pooler.ap-southeast-1.aws.neon.tech/neondb?sslmode=require
   ```

### Step 2: Push Your Code to GitHub
Make sure all your changes are committed and pushed to your GitHub repository:
```bash
git add .
git commit -m "Configure production deployment and enhanced frontend"
git push origin main
```

### Step 3: Deploy on Render
1. Log in to [render.com](https://render.com/).
2. In the Render Dashboard, click **New +** and select **Blueprint**.
3. Connect your GitHub repository (`sneha973/CPU-scheduling-simulator`).
4. Render will detect `render.yaml` and configure the web service automatically.
5. In the service setup screen, under **Environment Variables**, locate `DATABASE_URL` and paste your Neon connection string.
6. Click **Apply**. Render will:
   - Run `./build.sh` (installing dependencies, building the C++ engine, and collecting static files with WhiteNoise).
   - Run database migrations (`python manage.py migrate`) against Neon.
   - Start the service with Gunicorn (`scheduler_site.wsgi:application`).

### Step 4: Verify Deployment
Once the deploy status turns green (**Live**):
- Open your custom URL: `https://cpu-scheduling-simulator.onrender.com/`
- Check health endpoint: `https://cpu-scheduling-simulator.onrender.com/health/` (returns `{"status":"ok"}`)
- Run a simulation to verify database persistence and Gantt visualizations!

---

## Free-Tier Limitations to Keep in Mind

> [!NOTE]
> - **Render Web Service Inactivity Sleep**: Render's free web services automatically spin down (sleep) after **15 minutes of inactivity**. When a new visitor requests the site after it has been sleeping, Render wakes it up, which may take **30 to 50 seconds** for the first request. Subsequent requests will be fast and responsive.
> - **Neon Database Limits**: Neon's free tier provides 0.5 GB of storage and 100 compute hours per month, which is more than sufficient for academic projects, portfolios, and demonstrations. Neon databases automatically suspend compute when idle to conserve hours.
> - **Zero Cost**: Neither Render nor Neon will charge your account as long as you use the Free tier plans.

---

## Running Tests

Run the Django automated test suite:
```bash
python manage.py test
```

Tests verify:
- Accurate timeline calculations across FCFS, SJF, RR, and Priority
- Hand-checked execution orders and boundary conditions (idle intervals)
- Input validation (rejection of negative arrivals, zero bursts, duplicate IDs)
- Model and prediction integrity
- Persistent database run history
- CSRF protection and `/health/` health check

---

## API Documentation

### `POST /api/simulate/`
Simulates a workload across all four scheduling policies.

**Request Payload:**
```json
{
  "quantum": 2,
  "processes": [
    { "id": "P1", "arrival": 0, "burst": 5, "priority": 2 },
    { "id": "P2", "arrival": 1, "burst": 3, "priority": 1 }
  ]
}
```

**Response (200 OK):**
```json
{
  "id": 1,
  "created_at": "2026-11-26T01:00:00.000Z",
  "workload": [...],
  "quantum": 2,
  "results": {
    "FCFS": { "timeline": [...], "process_metrics": [...], "aggregate": {...} },
    "SJF": { ... },
    "RR": { ... },
    "PRIORITY": { ... }
  },
  "recommendation": {
    "algorithm": "SJF",
    "objective": "Lowest equal-weighted normalized average waiting time and response time",
    "score": 0.285,
    "model_status": "Decision tree trained on 2400 synthetic workloads; held-out accuracy 92.4%",
    "features": { ... }
  }
}
```

### `GET /api/history/`
Returns the 25 most recent simulation runs with workload and quantum for 1-click reloading.

### `GET /health/`
Returns `{"status": "ok"}` for container monitoring and uptime health checks.
