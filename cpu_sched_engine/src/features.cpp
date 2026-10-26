#include "schedsim/features.hpp"

#include <algorithm>
#include <cmath>

namespace schedsim {

const std::vector<std::string>& feature_names() {
    static const std::vector<std::string> names = {
        "num_processes",         // n
        "mean_burst",            // average burst time
        "burst_variance",        // population variance of burst times
        "burst_cv",              // std-dev / mean (scale-free burst spread)
        "burst_max_min_ratio",   // longest burst / shortest burst
        "arrival_span",          // last arrival - first arrival
        "arrival_density",       // n / (arrival_span + 1): processes per time unit of arrivals
        "mean_interarrival",     // arrival_span / (n - 1)
        "load_factor",           // total burst / (arrival_span + 1): >1 means a queue builds up
        "time_quantum",          // RR quantum
        "quantum_to_burst_ratio",// quantum / mean_burst
        "priority_variance",     // spread of priority values
        "priority_burst_corr",   // Pearson correlation(priority, burst); >0: long jobs have low priority
        "arrival_burst_corr",    // corr(arrival, burst); <0: long jobs come first (FCFS convoy), >0: short first
        "priority_arrival_corr", // corr(priority, arrival); >0: priority order == arrival order (Priority ~ FCFS)
    };
    return names;
}

namespace {

double mean(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return v.empty() ? 0.0 : s / static_cast<double>(v.size());
}

double variance(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double m = mean(v), s = 0;
    for (double x : v) s += (x - m) * (x - m);
    return s / static_cast<double>(v.size());
}

double correlation(const std::vector<double>& a, const std::vector<double>& b) {
    double va = variance(a), vb = variance(b);
    if (va <= 0 || vb <= 0) return 0.0;
    double ma = mean(a), mb = mean(b), cov = 0;
    for (size_t i = 0; i < a.size(); ++i) cov += (a[i] - ma) * (b[i] - mb);
    cov /= static_cast<double>(a.size());
    return cov / std::sqrt(va * vb);
}

}  // namespace

std::vector<double> extract_features(const Workload& w) {
    const size_t n = w.processes.size();
    std::vector<double> bursts, arrivals, prios;
    for (const auto& p : w.processes) {
        bursts.push_back(static_cast<double>(p.burst));
        arrivals.push_back(static_cast<double>(p.arrival));
        prios.push_back(static_cast<double>(p.priority));
    }

    const double dn = static_cast<double>(n);
    const double mean_b = mean(bursts);
    const double var_b = variance(bursts);
    const double min_b = *std::min_element(bursts.begin(), bursts.end());
    const double max_b = *std::max_element(bursts.begin(), bursts.end());
    const double span = *std::max_element(arrivals.begin(), arrivals.end()) -
                        *std::min_element(arrivals.begin(), arrivals.end());
    double total_burst = 0;
    for (double b : bursts) total_burst += b;

    std::vector<double> f;
    f.push_back(dn);
    f.push_back(mean_b);
    f.push_back(var_b);
    f.push_back(mean_b > 0 ? std::sqrt(var_b) / mean_b : 0.0);
    f.push_back(min_b > 0 ? max_b / min_b : 0.0);
    f.push_back(span);
    f.push_back(dn / (span + 1.0));
    f.push_back(n > 1 ? span / (dn - 1.0) : 0.0);
    f.push_back(total_burst / (span + 1.0));
    f.push_back(static_cast<double>(w.time_quantum));
    f.push_back(mean_b > 0 ? static_cast<double>(w.time_quantum) / mean_b : 0.0);
    f.push_back(variance(prios));
    f.push_back(correlation(prios, bursts));
    f.push_back(correlation(arrivals, bursts));
    f.push_back(correlation(prios, arrivals));
    return f;
}

std::vector<std::pair<std::string, double>> extract_named_features(const Workload& w) {
    const auto& names = feature_names();
    auto values = extract_features(w);
    std::vector<std::pair<std::string, double>> out;
    for (size_t i = 0; i < names.size(); ++i) out.emplace_back(names[i], values[i]);
    return out;
}

}  // namespace schedsim
