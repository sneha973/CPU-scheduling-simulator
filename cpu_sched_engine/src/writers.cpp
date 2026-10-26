#include "schedsim/writers.hpp"

#include <algorithm>
#include <sstream>

#include "schedsim/features.hpp"
#include "schedsim/util.hpp"

namespace schedsim {

namespace {

using util::fmt_double;

// ------------------------------------------------------------------ CSV helpers

std::string csv_cell(const std::string& s) {
    if (s.find_first_of(",\"\n\r") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    return out + "\"";
}

void csv_row(std::ostringstream& os, const std::vector<std::string>& cells) {
    for (size_t i = 0; i < cells.size(); ++i) {
        if (i) os << ',';
        os << csv_cell(cells[i]);
    }
    os << '\n';
}

std::string str(Time v) { return std::to_string(v); }

// ------------------------------------------------------------------ JSON -> XML

std::string xml_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

std::string item_name(const std::string& key) {
    if (key == "processes") return "process";
    if (key == "results") return "result";
    if (key == "gantt") return "segment";
    if (key == "algorithms") return "algorithm";
    if (key == "feature_names") return "name";
    if (key == "feature_vector") return "value";
    return "item";
}

void json_to_xml(std::ostringstream& os, const std::string& name, const Json& v, int depth) {
    const std::string pad(static_cast<size_t>(depth) * 2, ' ');
    switch (v.type()) {
        case Json::Type::Object:
            os << pad << '<' << name << ">\n";
            for (const auto& kv : v.members()) json_to_xml(os, kv.first, kv.second, depth + 1);
            os << pad << "</" << name << ">\n";
            break;
        case Json::Type::Array:
            os << pad << '<' << name << ">\n";
            for (const Json& item : v.items()) json_to_xml(os, item_name(name), item, depth + 1);
            os << pad << "</" << name << ">\n";
            break;
        case Json::Type::String: os << pad << '<' << name << '>' << xml_escape(v.as_string()) << "</" << name << ">\n"; break;
        case Json::Type::Number: os << pad << '<' << name << '>' << fmt_double(v.as_number()) << "</" << name << ">\n"; break;
        case Json::Type::Bool: os << pad << '<' << name << '>' << (v.as_bool() ? "true" : "false") << "</" << name << ">\n"; break;
        case Json::Type::Null: os << pad << '<' << name << "/>\n"; break;
    }
}

std::string to_xml(const Json& root, const std::string& root_name) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    json_to_xml(os, root_name, root, 0);
    return os.str();
}

// ------------------------------------------------------------------ text helpers

std::string pad_right(const std::string& s, size_t w) { return s.size() >= w ? s : s + std::string(w - s.size(), ' '); }
std::string pad_left(const std::string& s, size_t w) { return s.size() >= w ? s : std::string(w - s.size(), ' ') + s; }

std::string ascii_gantt(const std::vector<GanttSegment>& g) {
    std::string bars = "|", times;
    size_t pos = 1;
    times = std::string(1, ' ');
    // The timestamp row is built so each number starts exactly under its '|'.
    auto put_time = [&](size_t at, const std::string& t) {
        if (times.size() < at) times.resize(at, ' ');
        times += t;
    };
    put_time(0, std::to_string(g.front().start));
    for (const auto& s : g) {
        std::string cell = " " + s.pid + " ";
        const std::string endstr = std::to_string(s.end);
        if (cell.size() < endstr.size() + 1) cell += std::string(endstr.size() + 1 - cell.size(), ' ');
        bars += cell + "|";
        pos += cell.size() + 1;
        put_time(pos - 1, endstr);
    }
    return bars + "\n" + times;
}

std::string render_text(const Report& r) {
    std::ostringstream os;
    os << "CPU SCHEDULING SIMULATION  (engine " << kEngineVersion << ")\n";
    os << "Processes: " << r.workload.processes.size() << "   Time quantum: " << r.workload.time_quantum
       << "   Objective: " << r.objective << "\n\n";

    for (const auto& res : r.results) {
        os << "=== " << res.label << " ===\n";
        os << ascii_gantt(res.gantt) << "\n\n";
        os << pad_right("PID", 8) << pad_left("Arr", 6) << pad_left("Burst", 7) << pad_left("Prio", 6)
           << pad_left("Start", 7) << pad_left("Done", 7) << pad_left("WT", 6) << pad_left("TAT", 6)
           << pad_left("RT", 6) << "\n";
        for (const auto& p : res.processes) {
            os << pad_right(p.process.pid, 8) << pad_left(str(p.process.arrival), 6) << pad_left(str(p.process.burst), 7)
               << pad_left(str(p.process.priority), 6) << pad_left(str(p.first_start), 7)
               << pad_left(str(p.completion), 7) << pad_left(str(p.waiting), 6) << pad_left(str(p.turnaround), 6)
               << pad_left(str(p.response), 6) << "\n";
        }
        const Metrics& m = res.metrics;
        os << "Avg WT=" << fmt_double(m.avg_waiting_time) << "  Avg TAT=" << fmt_double(m.avg_turnaround_time)
           << "  Avg RT=" << fmt_double(m.avg_response_time) << "  CPU util=" << fmt_double(m.cpu_utilization)
           << "%  Context switches=" << m.context_switches << "\n\n";
    }

    os << "=== COMPARISON ===\n";
    os << pad_right("Algorithm", 28) << pad_left("AvgWT", 9) << pad_left("AvgTAT", 9) << pad_left("AvgRT", 9)
       << pad_left("CPU%", 9) << pad_left("CtxSw", 7) << pad_left("Score", 9) << "\n";
    for (size_t i = 0; i < r.results.size(); ++i) {
        const auto& res = r.results[i];
        os << pad_right(res.label, 28) << pad_left(fmt_double(res.metrics.avg_waiting_time), 9)
           << pad_left(fmt_double(res.metrics.avg_turnaround_time), 9)
           << pad_left(fmt_double(res.metrics.avg_response_time), 9)
           << pad_left(fmt_double(res.metrics.cpu_utilization), 9) << pad_left(std::to_string(res.metrics.context_switches), 7)
           << pad_left(fmt_double(util::round4(r.scores[i])), 9) << (static_cast<int>(i) == r.best_index ? "  <-- best" : "")
           << "\n";
    }
    os << "\nBest for objective '" << r.objective << "': " << r.results[static_cast<size_t>(r.best_index)].label << "\n";
    return os.str();
}

Json metrics_to_json(const Metrics& m) {
    Json j = Json::object();
    j.set("avg_waiting_time", m.avg_waiting_time);
    j.set("avg_turnaround_time", m.avg_turnaround_time);
    j.set("avg_response_time", m.avg_response_time);
    j.set("priority_weighted_waiting_time", m.priority_weighted_waiting_time);
    j.set("cpu_utilization", m.cpu_utilization);
    j.set("throughput", m.throughput);
    j.set("max_waiting_time", m.max_waiting_time);
    j.set("makespan", m.makespan);
    j.set("total_burst_time", m.total_burst_time);
    j.set("idle_time", m.idle_time);
    j.set("context_switches", m.context_switches);
    return j;
}

}  // namespace

// ------------------------------------------------------------------ public API

OutputFormat parse_output_format(const std::string& name) {
    const std::string n = util::lower(util::trim(name));
    if (n == "json" || n.empty()) return OutputFormat::Json;
    if (n == "csv") return OutputFormat::Csv;
    if (n == "text" || n == "txt") return OutputFormat::Text;
    if (n == "xml") return OutputFormat::Xml;
    throw EngineError("USAGE_ERROR", "Unknown output format '" + name + "'. Valid: json, csv, text, xml");
}

Json error_to_json(const std::string& code, const std::string& message) {
    Json j = Json::object();
    j.set("status", "error");
    j.set("code", code);
    j.set("message", message);
    return j;
}

Json report_to_json(const Report& r) {
    Json root = Json::object();
    root.set("status", "ok");
    root.set("engine_version", kEngineVersion);

    Json input = Json::object();
    input.set("time_quantum", r.workload.time_quantum);
    input.set("objective", r.objective);
    Json procs = Json::array();
    for (const auto& p : r.workload.processes) {
        Json pj = Json::object();
        pj.set("pid", p.pid);
        pj.set("arrival", p.arrival);
        pj.set("burst", p.burst);
        pj.set("priority", p.priority);
        procs.push(pj);
    }
    input.set("processes", procs);
    root.set("input", input);

    Json feats = Json::object();
    Json vec = Json::array();
    for (const auto& kv : r.features) {
        feats.set(kv.first, kv.second);
        vec.push(kv.second);
    }
    root.set("features", feats);
    root.set("feature_vector", vec);

    Json results = Json::array();
    for (size_t i = 0; i < r.results.size(); ++i) {
        const AlgorithmResult& res = r.results[i];
        Json rj = Json::object();
        rj.set("algorithm", res.algorithm);
        rj.set("label", res.label);
        rj.set("score", util::round4(r.scores[i]));
        rj.set("metrics", metrics_to_json(res.metrics));

        Json gantt = Json::array();
        for (const auto& s : res.gantt) {
            Json sj = Json::object();
            sj.set("pid", s.pid);
            sj.set("start", s.start);
            sj.set("end", s.end);
            gantt.push(sj);
        }
        rj.set("gantt", gantt);

        Json plist = Json::array();
        for (const auto& p : res.processes) {
            Json pj = Json::object();
            pj.set("pid", p.process.pid);
            pj.set("arrival", p.process.arrival);
            pj.set("burst", p.process.burst);
            pj.set("priority", p.process.priority);
            pj.set("first_start", p.first_start);
            pj.set("completion", p.completion);
            pj.set("waiting", p.waiting);
            pj.set("turnaround", p.turnaround);
            pj.set("response", p.response);
            plist.push(pj);
        }
        rj.set("processes", plist);
        results.push(rj);
    }
    root.set("results", results);

    const AlgorithmResult& best = r.results[static_cast<size_t>(r.best_index)];
    Json bj = Json::object();
    bj.set("algorithm", best.algorithm);
    bj.set("label", best.label);
    bj.set("objective", r.objective);
    bj.set("score", util::round4(r.scores[static_cast<size_t>(r.best_index)]));
    root.set("best", bj);
    return root;
}

std::string render_report(const Report& r, OutputFormat format, const RenderOptions& opts) {
    switch (format) {
        case OutputFormat::Json: return report_to_json(r).dump(opts.json_indent) + "\n";
        case OutputFormat::Xml: return to_xml(report_to_json(r), "simulation");
        case OutputFormat::Text: return render_text(r);
        case OutputFormat::Csv: break;
    }

    std::ostringstream os;
    if (opts.csv_view == "summary") {
        csv_row(os, {"algorithm", "label", "avg_waiting_time", "avg_turnaround_time", "avg_response_time",
                     "priority_weighted_waiting_time", "cpu_utilization", "throughput", "max_waiting_time", "makespan", "context_switches", "score",
                     "is_best"});
        for (size_t i = 0; i < r.results.size(); ++i) {
            const auto& res = r.results[i];
            const Metrics& m = res.metrics;
            csv_row(os, {res.algorithm, res.label, fmt_double(m.avg_waiting_time), fmt_double(m.avg_turnaround_time),
                         fmt_double(m.avg_response_time), fmt_double(m.priority_weighted_waiting_time),
                         fmt_double(m.cpu_utilization), fmt_double(m.throughput),
                         str(m.max_waiting_time), str(m.makespan), std::to_string(m.context_switches),
                         fmt_double(util::round4(r.scores[i])), static_cast<int>(i) == r.best_index ? "true" : "false"});
        }
    } else if (opts.csv_view == "processes") {
        csv_row(os, {"algorithm", "pid", "arrival", "burst", "priority", "first_start", "completion", "waiting",
                     "turnaround", "response"});
        for (const auto& res : r.results)
            for (const auto& p : res.processes)
                csv_row(os, {res.algorithm, p.process.pid, str(p.process.arrival), str(p.process.burst),
                             str(p.process.priority), str(p.first_start), str(p.completion), str(p.waiting),
                             str(p.turnaround), str(p.response)});
    } else if (opts.csv_view == "gantt") {
        csv_row(os, {"algorithm", "pid", "start", "end"});
        for (const auto& res : r.results)
            for (const auto& s : res.gantt) csv_row(os, {res.algorithm, s.pid, str(s.start), str(s.end)});
    } else {
        throw EngineError("USAGE_ERROR", "Unknown --view '" + opts.csv_view + "'. Valid: summary, processes, gantt");
    }
    return os.str();
}

Json features_to_json(const Workload& w) {
    Json root = Json::object();
    root.set("status", "ok");
    root.set("engine_version", kEngineVersion);
    Json names = Json::array();
    for (const auto& n : feature_names()) names.push(n);
    root.set("feature_names", names);
    Json feats = Json::object();
    Json vec = Json::array();
    for (const auto& kv : extract_named_features(w)) {
        feats.set(kv.first, kv.second);
        vec.push(kv.second);
    }
    root.set("features", feats);
    root.set("feature_vector", vec);
    return root;
}

std::string render_features(const Workload& w, OutputFormat format, const RenderOptions& opts) {
    switch (format) {
        case OutputFormat::Json: return features_to_json(w).dump(opts.json_indent) + "\n";
        case OutputFormat::Xml: return to_xml(features_to_json(w), "workload_features");
        case OutputFormat::Csv: {
            std::ostringstream os;
            csv_row(os, feature_names());
            std::vector<std::string> row;
            for (double v : extract_features(w)) row.push_back(fmt_double(v));
            csv_row(os, row);
            return os.str();
        }
        case OutputFormat::Text: {
            std::ostringstream os;
            for (const auto& kv : extract_named_features(w)) os << pad_right(kv.first, 26) << fmt_double(kv.second) << "\n";
            return os.str();
        }
    }
    return "";
}

std::string render_dataset(const Dataset& ds, OutputFormat format, bool with_metrics, const RenderOptions& opts) {
    const auto& names = feature_names();

    if (format == OutputFormat::Csv) {
        std::ostringstream os;
        std::vector<std::string> header = names;
        if (with_metrics)
            for (const auto& a : ds.algorithms) {
                header.push_back("avg_wt_" + a);
                header.push_back("avg_tat_" + a);
                header.push_back("avg_rt_" + a);
            }
        header.push_back("best_algorithm");
        csv_row(os, header);
        for (const auto& s : ds.samples) {
            std::vector<std::string> row;
            for (double v : s.features) row.push_back(fmt_double(v));
            if (with_metrics)
                for (const auto& m : s.metrics)
                    for (double v : m) row.push_back(fmt_double(v));
            row.push_back(s.best_algorithm);
            csv_row(os, row);
        }
        return os.str();
    }

    if (format == OutputFormat::Json || format == OutputFormat::Xml) {
        Json root = Json::object();
        root.set("status", "ok");
        root.set("engine_version", kEngineVersion);
        Json fn = Json::array();
        for (const auto& n : names) fn.push(n);
        root.set("feature_names", fn);
        Json classes = Json::object();
        for (const auto& kv : ds.class_counts) classes.set(kv.first, kv.second);
        root.set("class_counts", classes);
        Json samples = Json::array();
        for (const auto& s : ds.samples) {
            Json sj = Json::object();
            Json vec = Json::array();
            for (double v : s.features) vec.push(v);
            sj.set("features", vec);
            sj.set("best_algorithm", s.best_algorithm);
            if (with_metrics) {
                Json mj = Json::object();
                for (size_t a = 0; a < ds.algorithms.size(); ++a) {
                    Json one = Json::object();
                    one.set("avg_waiting_time", s.metrics[a][0]);
                    one.set("avg_turnaround_time", s.metrics[a][1]);
                    one.set("avg_response_time", s.metrics[a][2]);
                    mj.set(ds.algorithms[a], one);
                }
                sj.set("metrics", mj);
            }
            samples.push(sj);
        }
        root.set("samples", samples);
        return format == OutputFormat::Json ? root.dump(opts.json_indent) + "\n" : to_xml(root, "dataset");
    }

    throw EngineError("USAGE_ERROR", "Datasets can be written as csv, json or xml.");
}

}  // namespace schedsim
