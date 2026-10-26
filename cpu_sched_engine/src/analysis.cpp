#include "schedsim/analysis.hpp"

#include <algorithm>
#include <cmath>

#include "schedsim/features.hpp"
#include "schedsim/schedulers.hpp"

namespace schedsim {

bool is_valid_objective(const std::string& o) {
    return o == "wt" || o == "tat" || o == "rt" || o == "pwt" || o == "balanced";
}

std::vector<double> score_results(const std::vector<AlgorithmResult>& results, const std::string& objective) {
    if (!is_valid_objective(objective))
        throw EngineError("INVALID_INPUT", "Unknown objective '" + objective + "'. Valid: wt, tat, rt, pwt, balanced");

    std::vector<double> scores(results.size(), 0.0);
    if (objective != "balanced") {
        for (size_t i = 0; i < results.size(); ++i) {
            const Metrics& m = results[i].metrics;
            scores[i] = objective == "wt"    ? m.avg_waiting_time
                        : objective == "tat" ? m.avg_turnaround_time
                        : objective == "rt"  ? m.avg_response_time
                                             : m.priority_weighted_waiting_time;
        }
        return scores;
    }

    // balanced: each metric is expressed as a ratio to the best algorithm for that metric
    // (+1 smoothing so zeros are safe), then combined with fixed weights.
    constexpr double kWtW = 0.25, kTatW = 0.15, kRtW = 0.25, kFairW = 0.10, kPwW = 0.25;
    auto best_of = [&](auto getter) {
        double b = 1e300;
        for (const auto& r : results) b = std::min(b, static_cast<double>(getter(r.metrics)));
        return b;
    };
    const double b_wt = best_of([](const Metrics& m) { return m.avg_waiting_time; });
    const double b_tat = best_of([](const Metrics& m) { return m.avg_turnaround_time; });
    const double b_rt = best_of([](const Metrics& m) { return m.avg_response_time; });
    const double b_fair = best_of([](const Metrics& m) { return m.max_waiting_time; });
    const double b_pw = best_of([](const Metrics& m) { return m.priority_weighted_waiting_time; });

    for (size_t i = 0; i < results.size(); ++i) {
        const Metrics& m = results[i].metrics;
        scores[i] = kWtW * (m.avg_waiting_time + 1.0) / (b_wt + 1.0) +
                    kTatW * (m.avg_turnaround_time + 1.0) / (b_tat + 1.0) +
                    kRtW * (m.avg_response_time + 1.0) / (b_rt + 1.0) +
                    kFairW * (static_cast<double>(m.max_waiting_time) + 1.0) / (b_fair + 1.0) +
                    kPwW * (m.priority_weighted_waiting_time + 1.0) / (b_pw + 1.0);
    }
    return scores;
}

int pick_best(const std::vector<AlgorithmResult>& results, const std::vector<double>& scores) {
    constexpr double kEps = 1e-9;
    int best = 0;
    for (size_t i = 1; i < results.size(); ++i) {
        const size_t b = static_cast<size_t>(best);
        if (scores[i] < scores[b] - kEps) {
            best = static_cast<int>(i);
        } else if (std::fabs(scores[i] - scores[b]) <= kEps) {
            const int cs_i = results[i].metrics.context_switches, cs_b = results[b].metrics.context_switches;
            if (cs_i < cs_b ||
                (cs_i == cs_b && canonical_index(results[i].algorithm) < canonical_index(results[b].algorithm)))
                best = static_cast<int>(i);
        }
    }
    return best;
}

Report analyze(const Workload& w, const std::vector<std::string>& algorithms, const std::string& objective) {
    std::vector<std::string> algos = resolve_algorithms(algorithms);
    validate_workload(w, algos);
    if (!is_valid_objective(objective))
        throw EngineError("INVALID_INPUT", "Unknown objective '" + objective + "'. Valid: wt, tat, rt, pwt, balanced");

    Report rep;
    rep.workload = w;
    rep.objective = objective;
    rep.features = extract_named_features(w);
    for (const auto& id : algos) rep.results.push_back(run_algorithm(id, w));
    rep.scores = score_results(rep.results, objective);
    rep.best_index = pick_best(rep.results, rep.scores);
    return rep;
}

}  // namespace schedsim
