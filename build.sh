#!/usr/bin/env bash
set -euo pipefail

PYTHON_BIN="$(command -v python3 || command -v python)"

"$PYTHON_BIN" -m pip install --upgrade pip
"$PYTHON_BIN" -m pip install -r requirements.txt

# Compile C++17 engine if g++ is available; simulator falls back to Python reference engine otherwise
if command -v g++ >/dev/null 2>&1; then
  mkdir -p build
  g++ -std=c++17 -O2 -Wall -Wextra engine/main.cpp -o build/scheduler_engine
fi

"$PYTHON_BIN" manage.py collectstatic --no-input
