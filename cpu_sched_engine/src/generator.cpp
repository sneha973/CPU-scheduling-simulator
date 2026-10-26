#include "schedsim/generator.hpp"

#include <algorithm>
#include <cmath>
#include <random>

#include "schedsim/analysis.hpp"
#include "schedsim/features.hpp"
#include "schedsim/schedulers.hpp"

namespace schedsim {

namespace {

// mt19937_64's raw output is fully specified by the C++ standard, so unlike
// std::uniform_int_distribution it yields identical datasets on every compiler/OS.
class Rng {
public:
    explicit Rng(std::uint64_t seed) : eng_(seed) {}
    std::uint64_t next() { return eng_(); }
    Time range(Time lo, Time hi) {
        return lo + static_cast<Time>(next() % static_cast<std::uint64_t>(hi - lo + 1));
    }
    double real() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
    bool chance(double p) { return real() < p; }

private:
    std::mt19937_64 eng_;
};

Workload build_workload(Rng& rng, int min_n, int max_n) {
    const size_t n = static_cast<size_t>(rng.range(min_n, max_n));
    std::vector<Time> burst(n), arrival(n), prio(n);

    // ---- burst-time pattern
    const int burst_mode = static_cast<int>(rng.range(0, 5));
    switch (burst_mode) {
        case 0: {  // uniform
            Time hi = rng.range(5, 40);
            for (auto& b : burst) b = rng.range(1, hi);
            break;
        }
        case 1: {  // nearly equal
            Time base = rng.range(3, 15);
            for (auto& b : burst) b = std::max<Time>(1, base + rng.range(-1, 1));
            break;
        }
        case 2: {  // bimodal: many short jobs, a few long ones
            double p_short = 0.5 + 0.3 * rng.real();
            for (auto& b : burst) b = rng.chance(p_short) ? rng.range(1, 4) : rng.range(15, 50);
            break;
        }
        case 3: {  // heavy tail
            for (auto& b : burst)
                b = std::max<Time>(1, static_cast<Time>(std::floor(std::exp(rng.real() * std::log(80.0)))));
            break;
        }
        case 4: {  // identical
            Time base = rng.range(2, 15);
            for (auto& b : burst) b = base;
            break;
        }
        default: {  // long jobs arrive first (convoy effect)
            Time hi = rng.range(8, 40);
            for (auto& b : burst) b = rng.range(1, hi);
            std::sort(burst.begin(), burst.end(), std::greater<Time>());
            break;
        }
    }

    double mean_b = 0;
    for (Time b : burst) mean_b += static_cast<double>(b);
    mean_b /= static_cast<double>(n);

    // ---- arrival pattern
    const int arrival_mode = static_cast<int>(rng.range(0, 3));
    switch (arrival_mode) {
        case 0:  // everybody at t = 0
            for (auto& a : arrival) a = 0;
            break;
        case 1:  // tight cluster
            for (auto& a : arrival) a = rng.range(0, 5);
            break;
        case 2: {  // sparse, CPU may idle
            Time horizon = std::max<Time>(1, static_cast<Time>(static_cast<double>(n) * mean_b * (0.5 + 2.0 * rng.real())));
            for (auto& a : arrival) a = rng.range(0, horizon);
            break;
        }
        default: {  // steady trickle
            Time t = 0;
            for (auto& a : arrival) {
                a = t;
                t += rng.range(0, 4);
            }
            break;
        }
    }
    std::sort(arrival.begin(), arrival.end());

    // ---- priority pattern (1 = most urgent)
    std::vector<size_t> rank(n);
    for (size_t i = 0; i < n; ++i) rank[i] = i;
    std::stable_sort(rank.begin(), rank.end(), [&](size_t a, size_t b) { return burst[a] < burst[b]; });
    const int prio_mode = static_cast<int>(rng.range(0, 3));
    for (size_t r = 0; r < n; ++r) {
        size_t i = rank[r];
        Time scaled = n > 1 ? 1 + static_cast<Time>(9.0 * static_cast<double>(r) / static_cast<double>(n - 1)) : 1;
        switch (prio_mode) {
            case 0: prio[i] = rng.range(1, 10); break;                  // random
            case 1: prio[i] = scaled; break;                            // short jobs most urgent
            case 2: prio[i] = 11 - scaled; break;                       // long jobs most urgent
            default: prio[i] = 1; break;                                // no differentiation
        }
    }

    Workload w;
    for (size_t i = 0; i < n; ++i) {
        Process p;
        p.pid = "P" + std::to_string(i + 1);
        p.arrival = arrival[i];
        p.burst = burst[i];
        p.priority = prio[i];
        w.processes.push_back(p);
    }

    // ---- time quantum: sometimes a small fixed number, sometimes relative to the bursts
    if (rng.chance(0.5)) w.time_quantum = rng.range(1, 8);
    else w.time_quantum = std::max<Time>(1, static_cast<Time>(std::llround(mean_b * (0.1 + 1.4 * rng.real()))));
    return w;
}

}  // namespace

Workload random_workload(std::uint64_t seed, int min_processes, int max_processes) {
    Rng rng(seed);
    return build_workload(rng, min_processes, max_processes);
}

Dataset generate_dataset(const GeneratorConfig& cfg) {
    if (cfg.count == 0) throw EngineError("INVALID_INPUT", "count must be at least 1.");
    if (cfg.min_processes < 1 || cfg.max_processes < cfg.min_processes || cfg.max_processes > 1000)
        throw EngineError("INVALID_INPUT", "Need 1 <= min-procs <= max-procs <= 1000.");

    Dataset ds;
    ds.algorithms = resolve_algorithms(cfg.algorithms.empty() ? core_algorithm_ids() : cfg.algorithms);
    if (ds.algorithms.size() < 2)
        throw EngineError("INVALID_INPUT", "Need at least two algorithms to build a classification dataset.");
    if (!is_valid_objective(cfg.objective))
        throw EngineError("INVALID_INPUT", "Unknown objective '" + cfg.objective + "'. Valid: wt, tat, rt, balanced");

    Rng rng(cfg.seed);
    const size_t quota = (cfg.count + ds.algorithms.size() - 1) / ds.algorithms.size();
    const size_t max_attempts = cfg.count * 60;  // after this, class quotas are relaxed

    while (ds.samples.size() < cfg.count) {
        ++ds.attempts;
        Workload w = build_workload(rng, cfg.min_processes, cfg.max_processes);
        Report rep = analyze(w, ds.algorithms, cfg.objective);
        const std::string& label = rep.results[static_cast<size_t>(rep.best_index)].algorithm;

        if (cfg.balance && ds.attempts <= max_attempts && ds.class_counts[label] >= quota) continue;

        Sample s;
        s.workload = std::move(w);
        s.features = extract_features(s.workload);
        s.best_algorithm = label;
        for (const auto& r : rep.results)
            s.metrics.push_back({r.metrics.avg_waiting_time, r.metrics.avg_turnaround_time, r.metrics.avg_response_time});
        ds.class_counts[label]++;
        ds.samples.push_back(std::move(s));
    }
    return ds;
}

}  // namespace schedsim
