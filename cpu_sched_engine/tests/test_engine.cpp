// Self-contained test runner (no framework needed): `make test`
//
// Every expected number below was worked out by hand on paper (textbook workloads),
// so these tests verify the engine against ground truth, not against itself.

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>

#include "schedsim/analysis.hpp"
#include "schedsim/features.hpp"
#include "schedsim/generator.hpp"
#include "schedsim/json.hpp"
#include "schedsim/parsers.hpp"
#include "schedsim/schedulers.hpp"
#include "schedsim/util.hpp"
#include "schedsim/writers.hpp"

using namespace schedsim;

static int g_checks = 0, g_failed = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        ++g_checks;                                                                      \
        if (!(cond)) {                                                                   \
            ++g_failed;                                                                  \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond << "\n"; \
        }                                                                                \
    } while (0)

#define CHECK_NEAR(a, b) CHECK(std::fabs(static_cast<double>(a) - static_cast<double>(b)) < 1e-9)

#define CHECK_THROWS_CODE(expr, expected_code)                           \
    do {                                                                 \
        ++g_checks;                                                      \
        bool threw_expected_ = false;                                    \
        try { expr; } catch (const EngineError& e) { threw_expected_ = (e.code() == expected_code); } \
        if (!threw_expected_) { ++g_failed; std::cerr << "  FAIL " << __LINE__ << " expected " << expected_code << " from " #expr "\n"; } \
    } while (0)

static Workload make(std::initializer_list<std::array<Time, 4>> rows, Time quantum = 2) {
    Workload w;
    w.time_quantum = quantum;
    int i = 1;
    for (const auto& r : rows) w.processes.push_back({"P" + std::to_string(i++), r[0], r[1], r[2]});
    return w;
}
// rows are {arrival, burst, priority, unused}

static std::string gantt_str(const AlgorithmResult& r) {
    std::string s;
    for (const auto& g : r.gantt) s += g.pid + "[" + std::to_string(g.start) + "-" + std::to_string(g.end) + "] ";
    return s;
}

static const ProcessResult& find(const AlgorithmResult& r, const std::string& pid) {
    for (const auto& p : r.processes) if (p.process.pid == pid) return p;
    throw std::runtime_error("pid not found");
}

// ------------------------------------------------------------------ algorithm tests

static void test_fcfs_convoy() {
    // Silberschatz example: bursts 24, 3, 3 all at t=0 -> WT 0, 24, 27 (avg 17)
    Workload w = make({{0, 24, 0, 0}, {0, 3, 0, 0}, {0, 3, 0, 0}});
    auto r = run_algorithm("fcfs", w);
    CHECK(gantt_str(r) == "P1[0-24] P2[24-27] P3[27-30] ");
    CHECK_NEAR(r.metrics.avg_waiting_time, 17.0);
    CHECK_NEAR(r.metrics.avg_turnaround_time, 27.0);
    CHECK_NEAR(r.metrics.avg_response_time, 17.0);
    CHECK_NEAR(r.metrics.cpu_utilization, 100.0);
    CHECK(r.metrics.context_switches == 2);
}

static void test_sjf_nonpreemptive() {
    // P1(0,7) P2(2,4) P3(4,1) P4(5,4): order P1 P3 P2 P4, avg WT 4, avg TAT 8
    Workload w = make({{0, 7, 0, 0}, {2, 4, 0, 0}, {4, 1, 0, 0}, {5, 4, 0, 0}});
    auto r = run_algorithm("sjf", w);
    CHECK(gantt_str(r) == "P1[0-7] P3[7-8] P2[8-12] P4[12-16] ");
    CHECK_NEAR(r.metrics.avg_waiting_time, 4.0);
    CHECK_NEAR(r.metrics.avg_turnaround_time, 8.0);
}

static void test_srtf() {
    // Same workload, preemptive: avg WT 3.0 (P1 9, P2 1, P3 0, P4 2)
    Workload w = make({{0, 7, 0, 0}, {2, 4, 0, 0}, {4, 1, 0, 0}, {5, 4, 0, 0}});
    auto r = run_algorithm("srtf", w);
    CHECK(gantt_str(r) == "P1[0-2] P2[2-4] P3[4-5] P2[5-7] P4[7-11] P1[11-16] ");
    CHECK_NEAR(r.metrics.avg_waiting_time, 3.0);
    CHECK(find(r, "P1").waiting == 9);
    CHECK(find(r, "P3").response == 0);
}

static void test_round_robin() {
    // P1(0,5) P2(1,3) P3(2,1) P4(3,2) P5(4,3), q=2 -> avg WT 5.8, avg TAT 8.6, avg RT 2.4
    Workload w = make({{0, 5, 0, 0}, {1, 3, 0, 0}, {2, 1, 0, 0}, {3, 2, 0, 0}, {4, 3, 0, 0}}, 2);
    auto r = run_algorithm("rr", w);
    CHECK(gantt_str(r) == "P1[0-2] P2[2-4] P3[4-5] P1[5-7] P4[7-9] P5[9-11] P2[11-12] P1[12-13] P5[13-14] ");
    CHECK_NEAR(r.metrics.avg_waiting_time, 5.8);
    CHECK_NEAR(r.metrics.avg_turnaround_time, 8.6);
    CHECK_NEAR(r.metrics.avg_response_time, 2.4);
}

static void test_round_robin_textbook() {
    // Silberschatz: bursts 24, 3, 3, q=4 -> avg WT 17/3
    Workload w = make({{0, 24, 0, 0}, {0, 3, 0, 0}, {0, 3, 0, 0}}, 4);
    auto r = run_algorithm("rr", w);
    CHECK_NEAR(r.metrics.avg_waiting_time, util::round4(17.0 / 3.0));
    CHECK(find(r, "P2").waiting == 4);
    CHECK(find(r, "P3").waiting == 7);
    CHECK(find(r, "P1").waiting == 6);
}

static void test_round_robin_big_quantum_equals_fcfs() {
    Workload w = make({{0, 5, 0, 0}, {1, 3, 0, 0}, {2, 8, 0, 0}}, 100);
    auto rr = run_algorithm("rr", w), fc = run_algorithm("fcfs", w);
    CHECK(gantt_str(rr) == gantt_str(fc));
}

static void test_priority_nonpreemptive() {
    // Classic: bursts 10,1,2,1,5 priorities 3,1,4,5,2 -> order P2 P5 P1 P3 P4, avg WT 8.2
    Workload w = make({{0, 10, 3, 0}, {0, 1, 1, 0}, {0, 2, 4, 0}, {0, 1, 5, 0}, {0, 5, 2, 0}});
    auto r = run_algorithm("priority", w);
    CHECK(gantt_str(r) == "P2[0-1] P5[1-6] P1[6-16] P3[16-18] P4[18-19] ");
    CHECK_NEAR(r.metrics.avg_waiting_time, 8.2);
}

static void test_priority_preemptive() {
    // P1 (0, burst 4, prio 3) is preempted at t=1 by urgent P2 (1, burst 2, prio 1)
    Workload w = make({{0, 4, 3, 0}, {1, 2, 1, 0}});
    auto r = run_algorithm("priority_preemptive", w);
    CHECK(gantt_str(r) == "P1[0-1] P2[1-3] P1[3-6] ");
    CHECK(find(r, "P2").waiting == 0);
    CHECK(find(r, "P1").waiting == 2);
}

static void test_idle_gaps() {
    Workload w = make({{0, 2, 0, 0}, {5, 2, 0, 0}});
    for (const char* id : {"fcfs", "sjf", "rr", "priority", "srtf", "priority_preemptive"}) {
        auto r = run_algorithm(id, w);
        CHECK(gantt_str(r) == "P1[0-2] IDLE[2-5] P2[5-7] ");
        CHECK(r.metrics.idle_time == 3);
        CHECK_NEAR(r.metrics.cpu_utilization, util::round4(4.0 / 7.0 * 100.0));
        CHECK(r.metrics.context_switches == 1);
    }
}

static void test_late_first_arrival() {
    // CPU idle before the first arrival must not count against utilisation
    Workload w = make({{10, 4, 0, 0}});
    auto r = run_algorithm("fcfs", w);
    CHECK(gantt_str(r) == "P1[10-14] ");
    CHECK_NEAR(r.metrics.cpu_utilization, 100.0);
    CHECK(r.metrics.makespan == 4);
}

static void test_tie_breaking_is_stable() {
    // Equal arrival + equal burst: input order must win in every algorithm
    Workload w = make({{0, 3, 1, 0}, {0, 3, 1, 0}, {0, 3, 1, 0}});
    for (const char* id : {"fcfs", "sjf", "priority"}) {
        auto r = run_algorithm(id, w);
        CHECK(gantt_str(r) == "P1[0-3] P2[3-6] P3[6-9] ");
    }
}

static void test_universal_invariants() {
    // For ANY workload and ANY algorithm: TAT = WT + burst, RT <= WT, completions >= arrivals,
    // Gantt busy time == total burst, segments contiguous & ordered.
    for (std::uint64_t seed = 1; seed <= 150; ++seed) {
        Workload w = random_workload(seed, 1, 25);
        for (const auto& a : all_algorithms()) {
            auto r = run_algorithm(a.id, w);
            Time busy = 0, prev_end = r.gantt.front().start;
            bool contiguous = true;
            for (const auto& g : r.gantt) {
                contiguous &= (g.start == prev_end) && (g.end > g.start);
                prev_end = g.end;
                if (g.pid != kIdlePid) busy += g.end - g.start;
            }
            CHECK(contiguous);
            CHECK(busy == r.metrics.total_burst_time);
            for (const auto& p : r.processes) {
                CHECK(p.turnaround == p.waiting + p.process.burst);
                CHECK(p.waiting >= 0);
                CHECK(p.response >= 0 && p.response <= p.waiting);
                CHECK(p.completion >= p.process.arrival + p.process.burst);
            }
        }
    }
}

static void test_sjf_minimises_waiting_among_nonpreemptive() {
    // SJF is provably optimal for avg WT when all jobs arrive together.
    for (std::uint64_t seed = 1; seed <= 100; ++seed) {
        Workload w = random_workload(seed, 2, 15);
        for (auto& p : w.processes) p.arrival = 0;
        double sjf = run_algorithm("sjf", w).metrics.avg_waiting_time;
        CHECK(sjf <= run_algorithm("fcfs", w).metrics.avg_waiting_time + 1e-9);
        CHECK(sjf <= run_algorithm("rr", w).metrics.avg_waiting_time + 1e-9);
        CHECK(sjf <= run_algorithm("priority", w).metrics.avg_waiting_time + 1e-9);
    }
}

// ------------------------------------------------------------------ ranking / features

static void test_ranking() {
    Workload w = make({{0, 24, 0, 0}, {0, 3, 0, 0}, {0, 3, 0, 0}}, 4);
    Report rep = analyze(w, {"fcfs", "sjf"}, "wt");
    CHECK(rep.results[static_cast<size_t>(rep.best_index)].algorithm == "sjf");

    // identical outcome -> deterministic tie-break picks FCFS
    Workload eq = make({{0, 3, 1, 0}, {0, 3, 1, 0}});
    Report r2 = analyze(eq, {"sjf", "fcfs"}, "balanced");
    CHECK(r2.results[static_cast<size_t>(r2.best_index)].algorithm == "fcfs");

    // Priority wins the priority-weighted objective when urgency and burst length disagree:
    // P1 long+urgent (weight 9), P2 short+unimportant (weight 1).
    //   SJF:      P2 then P1 -> PWT = (9*5 + 1*0) / 10 = 4.5
    //   Priority: P1 then P2 -> PWT = (9*0 + 1*10) / 10 = 1.0
    Workload p = make({{0, 10, 1, 0}, {0, 5, 9, 0}});
    Report r3 = analyze(p, {"sjf", "priority"}, "pwt");
    CHECK_NEAR(r3.results[0].metrics.priority_weighted_waiting_time, 4.5);
    CHECK_NEAR(r3.results[1].metrics.priority_weighted_waiting_time, 1.0);
    CHECK(r3.results[static_cast<size_t>(r3.best_index)].algorithm == "priority");

    // with equal priorities the weighted metric collapses to plain average waiting time
    Workload q = make({{0, 4, 7, 0}, {1, 2, 7, 0}, {2, 6, 7, 0}});
    auto fq = run_algorithm("fcfs", q);
    CHECK_NEAR(fq.metrics.priority_weighted_waiting_time, fq.metrics.avg_waiting_time);
}

static void test_features() {
    Workload w = make({{0, 2, 1, 0}, {4, 4, 3, 0}, {8, 6, 5, 0}}, 3);
    auto f = extract_features(w);
    CHECK(f.size() == feature_names().size());
    auto at = [&](const std::string& n) {
        for (size_t i = 0; i < feature_names().size(); ++i) if (feature_names()[i] == n) return f[i];
        return -12345.0;
    };
    CHECK_NEAR(at("num_processes"), 3);
    CHECK_NEAR(at("mean_burst"), 4);
    CHECK_NEAR(at("burst_variance"), 8.0 / 3.0);   // ((2-4)^2 + 0 + (6-4)^2) / 3
    CHECK_NEAR(at("burst_max_min_ratio"), 3);
    CHECK_NEAR(at("arrival_span"), 8);
    CHECK_NEAR(at("quantum_to_burst_ratio"), 0.75);
    CHECK_NEAR(at("priority_burst_corr"), 1.0);    // perfectly correlated
    Workload single = make({{0, 5, 0, 0}});
    for (double v : extract_features(single)) CHECK(std::isfinite(v));
}

// ------------------------------------------------------------------ parsing

static std::string signature(const Request& r) {
    std::string s = "q=" + std::to_string(r.workload.time_quantum) + ";";
    for (const auto& p : r.workload.processes)
        s += p.pid + ":" + std::to_string(p.arrival) + "," + std::to_string(p.burst) + "," + std::to_string(p.priority) + ";";
    return s;
}

static void test_parsers_agree() {
    const std::string expect = "q=3;A:0,5,2;B:1,3,1;";
    auto json = parse_request(R"({"processes":[{"pid":"A","arrival":0,"burst":5,"priority":2},
                                  {"id":"B","arrival_time":1,"burst_time":3,"prio":1}],"time_quantum":3})", InputFormat::Auto);
    auto csv = parse_request("# quantum=3\nPID,Arrival Time,Burst Time,Priority\nA,0,5,2\nB,1,3,1\n", InputFormat::Auto);
    auto csv_nohdr = parse_request("quantum,3\nA,0,5,2\nB,1,3,1\n", InputFormat::Csv);
    auto txt = parse_request("quantum 3\nA 0 5 2\nB 1 3 1\n", InputFormat::Txt);
    auto txt_hdr = parse_request("PID AT BT PR\nA 0 5 2\nB 1 3 1\nquantum=3\n", InputFormat::Auto);
    auto xml = parse_request(R"(<?xml version="1.0"?><workload quantum="3">
                               <!-- comment --><process pid="A" arrival="0" burst="5" priority="2"/>
                               <process><id>B</id><arrival>1</arrival><burst>3</burst><priority>1</priority></process>
                               </workload>)", InputFormat::Auto);
    CHECK(signature(json) == expect);
    CHECK(signature(csv) == expect);
    CHECK(signature(csv_nohdr) == expect);
    CHECK(signature(txt) == expect);
    CHECK(signature(txt_hdr) == expect);
    CHECK(signature(xml) == expect);
}

static void test_parser_extras() {
    auto semi = parse_request("pid;arrival;burst\nA;0;5\nB;2;3\n", InputFormat::Csv);
    CHECK(semi.workload.processes.size() == 2);
    auto bom = parse_request("\xEF\xBB\xBFpid,arrival,burst\nA,0,5\n", InputFormat::Csv);
    CHECK(bom.workload.processes.size() == 1);
    auto cfg = parse_request(R"({"processes":[{"burst":4},{"burst":2}],"options":{"quantum":7,"objective":"WT"},
                                "algorithms":["fcfs","rr"]})", InputFormat::Json);
    CHECK(cfg.workload.time_quantum == 7);
    CHECK(cfg.workload.processes[1].pid == "P2");     // auto-named
    CHECK(cfg.workload.processes[0].arrival == 0);    // default
    CHECK(cfg.objective.value() == "wt");
    CHECK(cfg.algorithms->size() == 2);
    auto top_array = parse_request(R"([{"pid":"X","arrival":0,"burst":1}])", InputFormat::Auto);
    CHECK(top_array.workload.processes.size() == 1);
}

static void test_errors() {
    CHECK_THROWS_CODE(parse_request("", InputFormat::Auto), "PARSE_ERROR");
    CHECK_THROWS_CODE(parse_request("{not json", InputFormat::Json), "PARSE_ERROR");
    CHECK_THROWS_CODE(parse_request("<a><b></a>", InputFormat::Xml), "PARSE_ERROR");
    CHECK_THROWS_CODE(parse_request(R"({"processes":[{"arrival":0}]})", InputFormat::Json), "INVALID_INPUT");        // no burst
    CHECK_THROWS_CODE(parse_request(R"({"processes":[{"burst":2.5}]})", InputFormat::Json), "INVALID_INPUT");        // fractional
    CHECK_THROWS_CODE(parse_request(R"({"processes":[{"burst":"abc"}]})", InputFormat::Json), "INVALID_INPUT");
    CHECK_THROWS_CODE(parse_request(R"({"nothing":1})", InputFormat::Json), "INVALID_INPUT");
    CHECK_THROWS_CODE(parse_request("A 0\n", InputFormat::Txt), "INVALID_INPUT");

    Workload ok = make({{0, 3, 0, 0}});
    Workload neg = make({{-1, 3, 0, 0}});
    Workload zero = make({{0, 0, 0, 0}});
    Workload dup = make({{0, 3, 0, 0}, {0, 3, 0, 0}});
    dup.processes[1].pid = "P1";
    Workload empty;
    CHECK_THROWS_CODE(analyze(neg, {"fcfs"}, "wt"), "INVALID_INPUT");
    CHECK_THROWS_CODE(analyze(zero, {"fcfs"}, "wt"), "INVALID_INPUT");
    CHECK_THROWS_CODE(analyze(dup, {"fcfs"}, "wt"), "INVALID_INPUT");
    CHECK_THROWS_CODE(analyze(empty, {"fcfs"}, "wt"), "INVALID_INPUT");
    CHECK_THROWS_CODE(analyze(ok, {"bogus"}, "wt"), "INVALID_INPUT");
    CHECK_THROWS_CODE(analyze(ok, {"fcfs"}, "bogus"), "INVALID_INPUT");
    ok.time_quantum = 0;
    CHECK_THROWS_CODE(analyze(ok, {"rr"}, "wt"), "INVALID_INPUT");
    analyze(ok, {"fcfs"}, "wt");  // quantum is irrelevant when RR is not requested -> no throw
    CHECK_THROWS_CODE(parse_output_format("pdf"), "USAGE_ERROR");
}

// ------------------------------------------------------------------ output + generator

static void test_output_formats() {
    Workload w = make({{0, 5, 2, 0}, {1, 3, 1, 0}});
    Report rep = analyze(w, {"all"}, "balanced");
    RenderOptions ro;

    std::string js = render_report(rep, OutputFormat::Json, ro);
    Json parsed = Json::parse(js);                         // our own output must be valid JSON
    CHECK(parsed.find("status")->as_string() == "ok");
    CHECK(parsed.find("results")->items().size() == 4);
    CHECK(parsed.find("feature_vector")->items().size() == feature_names().size());
    CHECK(parsed.find("best") != nullptr);

    std::string xml = render_report(rep, OutputFormat::Xml, ro);
    CHECK(xml.find("<simulation>") != std::string::npos && xml.find("</simulation>") != std::string::npos);
    CHECK(xml.find("<segment>") != std::string::npos);

    auto line_count = [](const std::string& text) { return std::count(text.begin(), text.end(), '\n'); };
    ro.csv_view = "summary";   CHECK(line_count(render_report(rep, OutputFormat::Csv, ro)) == 5);   // header + 4 algorithms
    ro.csv_view = "processes"; CHECK(line_count(render_report(rep, OutputFormat::Csv, ro)) == 9);   // header + 4 x 2 processes
    ro.csv_view = "gantt";     CHECK(render_report(rep, OutputFormat::Csv, ro).rfind("algorithm,pid,start,end", 0) == 0);
    ro.csv_view = "nope";
    CHECK_THROWS_CODE(render_report(rep, OutputFormat::Csv, ro), "USAGE_ERROR");

    CHECK(render_report(rep, OutputFormat::Text, RenderOptions{}).find("COMPARISON") != std::string::npos);
}

static void test_json_roundtrip() {
    Json j = Json::parse(R"({"a":[1,2.5,-3e2,true,null,"x\n\u00e9\"y"],"b":{"c":{}}})");
    CHECK(j.find("a")->items().size() == 6);
    CHECK_NEAR(j.find("a")->items()[2].as_number(), -300);
    CHECK(j.find("a")->items()[5].as_string() == "x\n\xC3\xA9\"y");
    Json again = Json::parse(j.dump());
    CHECK(again.dump() == j.dump());
    Json pretty = Json::parse(j.dump(2));
    CHECK(pretty.dump() == j.dump());
    CHECK_THROWS_CODE(Json::parse("[1,]"), "PARSE_ERROR");
    CHECK_THROWS_CODE(Json::parse("{\"a\":1} x"), "PARSE_ERROR");
}

static void test_generator() {
    GeneratorConfig cfg;
    cfg.count = 300;
    cfg.seed = 99;
    Dataset a = generate_dataset(cfg), b = generate_dataset(cfg);
    CHECK(a.samples.size() == 300);
    CHECK(render_dataset(a, OutputFormat::Csv, false, {}) == render_dataset(b, OutputFormat::Csv, false, {}));  // reproducible
    size_t total = 0;
    for (const auto& kv : a.class_counts) total += kv.second;
    CHECK(total == 300);
    CHECK(a.class_counts.size() >= 3);                     // more than one class is actually produced
    for (const auto& s : a.samples) CHECK(s.features.size() == feature_names().size());

    cfg.balance = true;
    cfg.count = 400;
    Dataset bal = generate_dataset(cfg);
    for (const auto& kv : bal.class_counts) CHECK(kv.second <= 110);   // quota is 100 (+ relaxation slack)

    GeneratorConfig bad;
    bad.count = 0;
    CHECK_THROWS_CODE(generate_dataset(bad), "INVALID_INPUT");
}

int main() {
    struct T { const char* name; std::function<void()> fn; };
    const T tests[] = {
        {"FCFS convoy effect", test_fcfs_convoy},
        {"SJF non-preemptive", test_sjf_nonpreemptive},
        {"SRTF", test_srtf},
        {"Round Robin (5 procs)", test_round_robin},
        {"Round Robin (textbook)", test_round_robin_textbook},
        {"RR with huge quantum == FCFS", test_round_robin_big_quantum_equals_fcfs},
        {"Priority non-preemptive", test_priority_nonpreemptive},
        {"Priority preemptive", test_priority_preemptive},
        {"Idle gaps (all algorithms)", test_idle_gaps},
        {"Late first arrival", test_late_first_arrival},
        {"Stable tie-breaking", test_tie_breaking_is_stable},
        {"Invariants on 150 random workloads x 6 algorithms", test_universal_invariants},
        {"SJF optimal for avg WT (simultaneous arrival)", test_sjf_minimises_waiting_among_nonpreemptive},
        {"Ranking / objectives", test_ranking},
        {"Feature extraction", test_features},
        {"Parsers agree across JSON/CSV/TXT/XML", test_parsers_agree},
        {"Parser extras (BOM, ';', config, aliases)", test_parser_extras},
        {"Error handling", test_errors},
        {"Output formats", test_output_formats},
        {"JSON round-trip", test_json_roundtrip},
        {"Dataset generator", test_generator},
    };
    for (const auto& t : tests) {
        int before = g_failed;
        try {
            t.fn();
        } catch (const std::exception& e) {
            ++g_failed;
            std::cerr << "  EXCEPTION in " << t.name << ": " << e.what() << "\n";
        }
        std::cout << (g_failed == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << "\n" << g_checks << " checks, " << g_failed << " failed\n";
    return g_failed == 0 ? 0 : 1;
}
