// Core data types shared by every module of the scheduling engine.
#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace schedsim {

using Time = long long;

inline constexpr const char* kEngineVersion = "1.0.0";
inline constexpr const char* kIdlePid = "IDLE";

// Every error that should reach the caller as a clean {"status":"error"} reply.
// code examples: PARSE_ERROR, INVALID_INPUT, IO_ERROR, USAGE_ERROR
class EngineError : public std::runtime_error {
public:
    EngineError(std::string code, const std::string& message)
        : std::runtime_error(message), code_(std::move(code)) {}
    const std::string& code() const { return code_; }

private:
    std::string code_;
};

struct Process {
    std::string pid;
    Time arrival = 0;
    Time burst = 1;
    Time priority = 0;  // lower number = higher priority
};

struct Workload {
    std::vector<Process> processes;
    Time time_quantum = 2;  // used by Round Robin only
};

struct GanttSegment {
    std::string pid;  // "IDLE" when the CPU has nothing to run
    Time start = 0;
    Time end = 0;
};

struct ProcessResult {
    Process process;
    Time first_start = 0;
    Time completion = 0;
    Time waiting = 0;
    Time turnaround = 0;
    Time response = 0;
};

struct Metrics {
    double avg_waiting_time = 0;
    double avg_turnaround_time = 0;
    double avg_response_time = 0;
    // Waiting time where urgent processes (small priority number) count more.
    // Equals avg_waiting_time when all priorities are equal.
    double priority_weighted_waiting_time = 0;
    double cpu_utilization = 0;  // percent, busy / makespan * 100
    double throughput = 0;       // processes per time unit
    Time max_waiting_time = 0;
    Time makespan = 0;           // last completion - first arrival
    Time total_burst_time = 0;
    Time idle_time = 0;
    int context_switches = 0;
};

struct AlgorithmResult {
    std::string algorithm;  // machine id, e.g. "fcfs"
    std::string label;      // human name, e.g. "FCFS"
    std::vector<GanttSegment> gantt;
    std::vector<ProcessResult> processes;
    Metrics metrics;
};

// Full answer of one "simulate" call.
struct Report {
    Workload workload;
    std::vector<std::pair<std::string, double>> features;  // ordered
    std::vector<AlgorithmResult> results;
    std::string objective;
    std::vector<double> scores;  // aligned with results, lower = better
    int best_index = -1;
};

// What a parsed input file asks for. Anything left empty falls back to defaults / CLI flags.
struct Request {
    Workload workload;
    std::optional<std::vector<std::string>> algorithms;
    std::optional<std::string> objective;
};

}  // namespace schedsim
