// Runs several algorithms on one workload and ranks them against an objective.
#pragma once

#include <string>
#include <vector>

#include "schedsim/types.hpp"

namespace schedsim {

// Valid objectives:
//   "wt"  average waiting time          "tat" average turnaround time
//   "rt"  average response time         "pwt" priority-weighted waiting time
//   "balanced" weighted mix: 25% WT, 15% TAT, 25% RT, 10% worst-case WT (fairness), 25% PWT
//              (each metric is first divided by the best value any algorithm achieves).
bool is_valid_objective(const std::string& objective);

// Lower score = better. Aligned with `results`.
std::vector<double> score_results(const std::vector<AlgorithmResult>& results, const std::string& objective);

// Index of the winner; ties are broken by fewer context switches, then canonical order.
int pick_best(const std::vector<AlgorithmResult>& results, const std::vector<double>& scores);

// Validate -> simulate every requested algorithm -> extract features -> rank.
Report analyze(const Workload& w, const std::vector<std::string>& algorithms, const std::string& objective);

}  // namespace schedsim
