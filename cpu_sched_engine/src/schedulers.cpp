#include "schedsim/schedulers.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <set>

#include "schedsim/util.hpp"

namespace schedsim {

namespace {

constexpr size_t kMaxProcesses = 5000;
constexpr Time kMaxValue = 1000000;          // arrival / burst / quantum upper bound
constexpr Time kMaxRoundRobinSlices = 5000000;

const std::vector<AlgorithmInfo>& registry() {
    static const std::vector<AlgorithmInfo> r = {
        {"fcfs", "FCFS", true},
        {"sjf", "SJF (Non-Preemptive)", true},
        {"srtf", "SRTF (Preemptive SJF)", false},
        {"priority", "Priority (Non-Preemptive)", true},
        {"priority_preemptive", "Priority (Preemptive)", false},
        {"rr", "Round Robin", true},
    };
    return r;
}

struct Job {
    size_t idx = 0;
    Time arrival = 0;
    Time burst = 0;
    Time remaining = 0;
    Time priority = 0;
    Time first_start = -1;
    Time completion = -1;
};

using Key = std::array<Time, 3>;
using KeyFn = Key (*)(const Job&);

std::vector<Job> make_jobs(const Workload& w) {
    std::vector<Job> jobs(w.processes.size());
    for (size_t i = 0; i < jobs.size(); ++i) {
        const Process& p = w.processes[i];
        jobs[i] = Job{i, p.arrival, p.burst, p.burst, p.priority, -1, -1};
    }
    return jobs;
}

// Appends [s,e) to the Gantt chart, merging with the previous slice if it is the same
// process and contiguous (so RR with a single ready process shows one long block).
void add_segment(std::vector<GanttSegment>& g, const std::string& pid, Time s, Time e) {
    if (e <= s) return;
    if (!g.empty() && g.back().pid == pid && g.back().end == s) {
        g.back().end = e;
        return;
    }
    g.push_back({pid, s, e});
}

Time earliest_arrival(const std::vector<Job>& jobs) {
    Time t = jobs.front().arrival;
    for (const auto& j : jobs) t = std::min(t, j.arrival);
    return t;
}

// Shared engine for FCFS, SJF, SRTF and both Priority variants. At every decision point the
// ready job with the smallest key runs. Preemptive variants re-decide whenever a new job
// arrives; non-preemptive variants run the chosen job to completion.
std::vector<GanttSegment> run_selection(std::vector<Job>& jobs, const Workload& w, KeyFn key,
                                        bool preemptive) {
    std::vector<GanttSegment> gantt;
    size_t done = 0;
    Time t = earliest_arrival(jobs);

    while (done < jobs.size()) {
        int best = -1;
        Key best_key{};
        for (size_t i = 0; i < jobs.size(); ++i) {
            const Job& j = jobs[i];
            if (j.remaining <= 0 || j.arrival > t) continue;
            Key k = key(j);
            if (best < 0 || k < best_key) {
                best = static_cast<int>(i);
                best_key = k;
            }
        }

        // Next arrival strictly after t among unfinished jobs (if any).
        Time next_arrival = -1;
        for (const Job& j : jobs)
            if (j.remaining > 0 && j.arrival > t && (next_arrival < 0 || j.arrival < next_arrival))
                next_arrival = j.arrival;

        if (best < 0) {  // CPU idle until the next job shows up
            add_segment(gantt, kIdlePid, t, next_arrival);
            t = next_arrival;
            continue;
        }

        Job& j = jobs[static_cast<size_t>(best)];
        if (j.first_start < 0) j.first_start = t;
        Time end = t + j.remaining;
        if (preemptive && next_arrival >= 0 && next_arrival < end) end = next_arrival;

        add_segment(gantt, w.processes[j.idx].pid, t, end);
        j.remaining -= (end - t);
        t = end;
        if (j.remaining == 0) {
            j.completion = t;
            ++done;
        }
    }
    return gantt;
}

std::vector<GanttSegment> run_round_robin(std::vector<Job>& jobs, const Workload& w) {
    std::vector<GanttSegment> gantt;
    const Time quantum = w.time_quantum;

    std::vector<size_t> order(jobs.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return jobs[a].arrival < jobs[b].arrival;
    });

    std::deque<size_t> ready;
    size_t next = 0, done = 0;
    Time t = earliest_arrival(jobs);

    // Convention: jobs that arrive while a slice runs are queued BEFORE the preempted job.
    auto admit = [&]() {
        while (next < order.size() && jobs[order[next]].arrival <= t) ready.push_back(order[next++]);
    };
    admit();

    while (done < jobs.size()) {
        if (ready.empty()) {  // idle gap
            Time na = jobs[order[next]].arrival;
            add_segment(gantt, kIdlePid, t, na);
            t = na;
            admit();
            continue;
        }
        size_t i = ready.front();
        ready.pop_front();
        Job& j = jobs[i];
        if (j.first_start < 0) j.first_start = t;

        Time run = std::min(quantum, j.remaining);
        add_segment(gantt, w.processes[i].pid, t, t + run);
        t += run;
        j.remaining -= run;
        admit();
        if (j.remaining == 0) {
            j.completion = t;
            ++done;
        } else {
            ready.push_back(i);
        }
    }
    return gantt;
}

int count_context_switches(const std::vector<GanttSegment>& gantt) {
    int switches = 0;
    std::string last;
    for (const auto& s : gantt) {
        if (s.pid == kIdlePid) continue;
        if (!last.empty() && s.pid != last) ++switches;
        last = s.pid;
    }
    return switches;
}

AlgorithmResult finalize(const std::string& id, const Workload& w, const std::vector<Job>& jobs,
                         std::vector<GanttSegment> gantt) {
    AlgorithmResult res;
    res.algorithm = id;
    for (const auto& a : registry())
        if (a.id == id) res.label = a.label;
    res.gantt = std::move(gantt);

    const size_t n = jobs.size();
    double sum_wt = 0, sum_tat = 0, sum_rt = 0, sum_pw = 0, sum_w = 0;
    Time max_prio = jobs.front().priority;
    for (const auto& j : jobs) max_prio = std::max(max_prio, j.priority);
    Time first_arrival = earliest_arrival(jobs), last_completion = 0, total_burst = 0, max_wt = 0;

    for (size_t i = 0; i < n; ++i) {
        const Job& j = jobs[i];
        ProcessResult pr;
        pr.process = w.processes[i];
        pr.first_start = j.first_start;
        pr.completion = j.completion;
        pr.turnaround = j.completion - j.arrival;
        pr.waiting = pr.turnaround - j.burst;
        pr.response = j.first_start - j.arrival;
        res.processes.push_back(pr);

        sum_wt += static_cast<double>(pr.waiting);
        sum_tat += static_cast<double>(pr.turnaround);
        sum_rt += static_cast<double>(pr.response);
        const double weight = static_cast<double>(max_prio - j.priority + 1);  // most urgent = largest
        sum_pw += weight * static_cast<double>(pr.waiting);
        sum_w += weight;
        last_completion = std::max(last_completion, j.completion);
        total_burst += j.burst;
        max_wt = std::max(max_wt, pr.waiting);
    }

    Metrics& m = res.metrics;
    const double dn = static_cast<double>(n);
    m.avg_waiting_time = util::round4(sum_wt / dn);
    m.avg_turnaround_time = util::round4(sum_tat / dn);
    m.avg_response_time = util::round4(sum_rt / dn);
    m.priority_weighted_waiting_time = util::round4(sum_pw / sum_w);
    m.makespan = last_completion - first_arrival;
    m.total_burst_time = total_burst;
    m.idle_time = m.makespan - total_burst;
    m.cpu_utilization = util::round4(static_cast<double>(total_burst) / static_cast<double>(m.makespan) * 100.0);
    m.throughput = util::round4(dn / static_cast<double>(m.makespan));
    m.max_waiting_time = max_wt;
    m.context_switches = count_context_switches(res.gantt);
    return res;
}

}  // namespace

// ------------------------------------------------------------------ public API

const std::vector<AlgorithmInfo>& all_algorithms() { return registry(); }

std::vector<std::string> core_algorithm_ids() { return {"fcfs", "sjf", "rr", "priority"}; }

int canonical_index(const std::string& id) {
    const auto& r = registry();
    for (size_t i = 0; i < r.size(); ++i)
        if (r[i].id == id) return static_cast<int>(i);
    return -1;
}

std::vector<std::string> resolve_algorithms(const std::vector<std::string>& requested) {
    std::vector<std::string> out;
    auto add = [&](const std::string& id) {
        if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    };
    for (const auto& raw : requested) {
        std::string id = util::lower(util::trim(raw));
        if (id.empty()) continue;
        if (id == "all") {
            for (const auto& c : core_algorithm_ids()) add(c);
        } else if (id == "extended") {
            for (const auto& a : registry()) add(a.id);
        } else if (id == "roundrobin" || id == "round_robin") {
            add("rr");
        } else if (id == "sjf_preemptive") {
            add("srtf");
        } else if (canonical_index(id) >= 0) {
            add(id);
        } else {
            throw EngineError("INVALID_INPUT",
                              "Unknown algorithm '" + raw +
                                  "'. Valid: fcfs, sjf, srtf, priority, priority_preemptive, rr, all, extended");
        }
    }
    if (out.empty()) out = core_algorithm_ids();
    return out;
}

void validate_workload(const Workload& w, const std::vector<std::string>& algorithms) {
    if (w.processes.empty()) throw EngineError("INVALID_INPUT", "The workload contains no processes.");
    if (w.processes.size() > kMaxProcesses)
        throw EngineError("INVALID_INPUT",
                          "Too many processes (max " + std::to_string(kMaxProcesses) + ").");

    std::set<std::string> seen;
    for (const auto& p : w.processes) {
        if (p.pid.empty()) throw EngineError("INVALID_INPUT", "A process has an empty pid.");
        if (p.pid == kIdlePid)
            throw EngineError("INVALID_INPUT", "The pid 'IDLE' is reserved.");
        if (!seen.insert(p.pid).second)
            throw EngineError("INVALID_INPUT", "Duplicate process id '" + p.pid + "'.");
        if (p.arrival < 0)
            throw EngineError("INVALID_INPUT", "Process '" + p.pid + "': arrival time cannot be negative.");
        if (p.burst < 1)
            throw EngineError("INVALID_INPUT", "Process '" + p.pid + "': burst time must be at least 1.");
        if (p.arrival > kMaxValue || p.burst > kMaxValue)
            throw EngineError("INVALID_INPUT",
                              "Process '" + p.pid + "': arrival/burst must not exceed " + std::to_string(kMaxValue) + ".");
    }

    if (std::find(algorithms.begin(), algorithms.end(), "rr") != algorithms.end()) {
        if (w.time_quantum < 1)
            throw EngineError("INVALID_INPUT", "time_quantum must be at least 1 for Round Robin.");
        if (w.time_quantum > kMaxValue)
            throw EngineError("INVALID_INPUT", "time_quantum must not exceed " + std::to_string(kMaxValue) + ".");
        Time slices = 0;
        for (const auto& p : w.processes) slices += (p.burst + w.time_quantum - 1) / w.time_quantum;
        if (slices > kMaxRoundRobinSlices)
            throw EngineError("INVALID_INPUT",
                              "Round Robin would need too many time slices; increase the time quantum.");
    }
}

AlgorithmResult run_algorithm(const std::string& id, const Workload& w) {
    std::vector<Job> jobs = make_jobs(w);
    std::vector<GanttSegment> gantt;

    if (id == "fcfs") {
        gantt = run_selection(jobs, w, [](const Job& j) { return Key{j.arrival, static_cast<Time>(j.idx), 0}; }, false);
    } else if (id == "sjf" || id == "srtf") {
        gantt = run_selection(jobs, w,
                              [](const Job& j) { return Key{j.remaining, j.arrival, static_cast<Time>(j.idx)}; },
                              id == "srtf");
    } else if (id == "priority" || id == "priority_preemptive") {
        gantt = run_selection(jobs, w,
                              [](const Job& j) { return Key{j.priority, j.arrival, static_cast<Time>(j.idx)}; },
                              id == "priority_preemptive");
    } else if (id == "rr") {
        gantt = run_round_robin(jobs, w);
    } else {
        throw EngineError("INVALID_INPUT", "Unknown algorithm '" + id + "'.");
    }
    return finalize(id, w, jobs, std::move(gantt));
}

}  // namespace schedsim
