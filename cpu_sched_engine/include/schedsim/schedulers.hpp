// CPU scheduling algorithms + input validation.
#pragma once

#include <string>
#include <vector>

#include "schedsim/types.hpp"

namespace schedsim {

struct AlgorithmInfo {
    std::string id;     // machine name used in JSON / CLI / ML labels
    std::string label;  // display name
    bool core;          // part of the four algorithms named in the project synopsis
};

// Canonical order: fcfs, sjf, srtf, priority, priority_preemptive, rr
const std::vector<AlgorithmInfo>& all_algorithms();

// The four synopsis algorithms: fcfs, sjf, rr, priority
std::vector<std::string> core_algorithm_ids();

// Expands "all" -> core four, "extended" -> all six, validates ids, removes duplicates.
std::vector<std::string> resolve_algorithms(const std::vector<std::string>& requested);

// Position in the canonical order (used for deterministic tie-breaking).
int canonical_index(const std::string& id);

// Throws EngineError("INVALID_INPUT") for empty workloads, bad values, duplicate pids, etc.
void validate_workload(const Workload& w, const std::vector<std::string>& algorithms);

// Runs one algorithm. The workload must already be validated.
AlgorithmResult run_algorithm(const std::string& id, const Workload& w);

}  // namespace schedsim
