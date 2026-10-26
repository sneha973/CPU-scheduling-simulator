// Synthetic workload generator + auto-labeller: produces the ML training dataset.
//
// Every sample is a random workload whose label ("best_algorithm") is obtained by actually
// running the simulators and ranking them with the chosen objective, i.e. the label is
// ground truth computed by this engine - not a guess.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "schedsim/types.hpp"

namespace schedsim {

struct GeneratorConfig {
    size_t count = 1000;
    std::uint64_t seed = 42;
    int min_processes = 3;
    int max_processes = 20;
    std::string objective = "balanced";
    std::vector<std::string> algorithms;  // empty -> core four
    bool balance = false;                 // try to equalise class counts
};

struct Sample {
    Workload workload;
    std::vector<double> features;  // order = feature_names()
    std::string best_algorithm;
    // avg waiting / turnaround / response per algorithm (same order as Dataset::algorithms)
    std::vector<std::vector<double>> metrics;
};

struct Dataset {
    std::vector<std::string> algorithms;
    std::vector<Sample> samples;
    std::map<std::string, size_t> class_counts;
    size_t attempts = 0;
};

Dataset generate_dataset(const GeneratorConfig& cfg);

// A single random workload (exposed for tests / custom pipelines).
Workload random_workload(std::uint64_t seed, int min_processes, int max_processes);

}  // namespace schedsim
