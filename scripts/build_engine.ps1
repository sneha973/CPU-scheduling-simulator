$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path 'build' | Out-Null
g++ -std=c++17 -O2 -Wall -Wextra engine/main.cpp -o build/scheduler_engine.exe
if ($LASTEXITCODE -ne 0) { throw 'C++ engine compilation failed.' }
Write-Host 'Built build/scheduler_engine.exe'
