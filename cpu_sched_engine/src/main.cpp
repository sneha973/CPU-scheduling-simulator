// schedsim - command line front-end of the CPU scheduling engine.
//
//   schedsim simulate  [--input F] [--in-format auto|json|csv|txt|xml] [--output F]
//                      [--out-format json|csv|text|xml] [--algos fcfs,sjf,rr,priority]
//                      [--quantum N] [--objective balanced] [--view summary] [--pretty]
//   schedsim features  [--input F] ...                (feature vector for the ML model)
//   schedsim generate  --count N [--seed S] [--output F] [--format csv|json|xml] ...
//   schedsim algorithms | --help | --version
//
// Exit code 0 = success, 1 = error (details as JSON on stdout, or text on stderr).

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

#include "schedsim/analysis.hpp"
#include "schedsim/generator.hpp"
#include "schedsim/parsers.hpp"
#include "schedsim/schedulers.hpp"
#include "schedsim/util.hpp"
#include "schedsim/writers.hpp"

using namespace schedsim;

namespace {

const char* kUsage = R"(schedsim - CPU scheduling engine (v)" "1.0.0" R"()

USAGE
  schedsim simulate  [options]     run algorithms on a workload
  schedsim features  [options]     print the ML feature vector of a workload
  schedsim generate  [options]     create a labelled synthetic dataset for ML training
  schedsim algorithms              list algorithm ids
  schedsim --help | --version

INPUT OPTIONS (simulate, features)
  --input FILE         workload file, or '-' / omitted for stdin
  --in-format FMT      auto (default) | json | csv | txt | xml
  --algos LIST         comma list of: fcfs sjf srtf priority priority_preemptive rr
                       or 'all' (= fcfs,sjf,rr,priority; default) or 'extended' (all six)
  --quantum N          Round Robin time quantum (overrides the file)
  --objective OBJ      balanced (default) | wt | tat | rt | pwt

OUTPUT OPTIONS
  --output FILE        write to FILE instead of stdout
  --out-format FMT     json (default) | csv | text | xml
  --view V             CSV only: summary (default) | processes | gantt
  --pretty             indent JSON output

GENERATE OPTIONS
  --count N            number of samples (default 1000)
  --seed S             RNG seed, same seed = same dataset on every platform (default 42)
  --min-procs N / --max-procs N   workload size range (default 3 / 20)
  --format FMT         csv (default) | json | xml
  --balance            try to make all classes equally frequent
  --with-metrics       also write per-algorithm avg WT/TAT/RT columns
  --algos, --objective as above
)";

struct Args {
    std::string command;
    std::map<std::string, std::string> values;
    std::set<std::string> flags;

    bool has(const std::string& k) const { return values.count(k) > 0; }
    std::string get(const std::string& k, const std::string& def = "") const {
        auto it = values.find(k);
        return it == values.end() ? def : it->second;
    }
};

const std::set<std::string> kFlagOptions = {"pretty", "balance", "with-metrics", "help", "version"};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string tok = argv[i];
        if (tok == "-h") tok = "--help";
        if (tok == "-v") tok = "--version";
        if (tok.rfind("--", 0) == 0) {
            std::string key = tok.substr(2), value;
            auto eq = key.find('=');
            if (eq != std::string::npos) {
                value = key.substr(eq + 1);
                key = key.substr(0, eq);
                a.values[key] = value;
            } else if (kFlagOptions.count(key)) {
                a.flags.insert(key);
            } else {
                if (i + 1 >= argc) throw EngineError("USAGE_ERROR", "Option --" + key + " needs a value.");
                a.values[key] = argv[++i];
            }
        } else if (a.command.empty()) {
            a.command = tok;
        } else {
            throw EngineError("USAGE_ERROR", "Unexpected argument '" + tok + "'.");
        }
    }
    return a;
}

std::string read_all(const std::string& path) {
    if (path.empty() || path == "-") {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        return ss.str();
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) throw EngineError("IO_ERROR", "Cannot open input file '" + path + "'.");
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void write_all(const std::string& path, const std::string& data) {
    if (path.empty()) {
        std::cout << data;
        std::cout.flush();
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) throw EngineError("IO_ERROR", "Cannot open output file '" + path + "' for writing.");
    out << data;
    if (!out) throw EngineError("IO_ERROR", "Failed while writing '" + path + "'.");
}

Time to_int_option(const Args& a, const std::string& key, const std::string& what) {
    auto v = util::parse_time(a.get(key));
    if (!v) throw EngineError("USAGE_ERROR", "--" + key + " must be an integer (" + what + ").");
    return *v;
}

std::vector<std::string> split_algos(const std::string& s) {
    std::vector<std::string> out;
    for (auto& p : util::split(s, ',')) {
        std::string t = util::trim(p);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

// Reads + parses the workload and merges CLI overrides.
struct Prepared {
    Workload workload;
    std::vector<std::string> algorithms;
    std::string objective;
};

Prepared prepare(const Args& a) {
    const std::string path = a.get("input");
    const std::string content = read_all(path);
    Request req = parse_request(content, parse_input_format(a.get("in-format", "auto")), path == "-" ? "" : path);

    Prepared p;
    p.workload = req.workload;
    if (a.has("quantum")) p.workload.time_quantum = to_int_option(a, "quantum", "time quantum");

    if (a.has("algos")) p.algorithms = split_algos(a.get("algos"));
    else if (req.algorithms) p.algorithms = *req.algorithms;
    p.algorithms = resolve_algorithms(p.algorithms);

    p.objective = a.has("objective") ? util::lower(a.get("objective")) : req.objective.value_or("balanced");
    return p;
}

int run_simulate(const Args& a, OutputFormat out_fmt, const RenderOptions& ro) {
    Prepared p = prepare(a);
    Report report = analyze(p.workload, p.algorithms, p.objective);
    write_all(a.get("output"), render_report(report, out_fmt, ro));
    return 0;
}

int run_features(const Args& a, OutputFormat out_fmt, const RenderOptions& ro) {
    Prepared p = prepare(a);
    validate_workload(p.workload, {});  // structural checks only
    write_all(a.get("output"), render_features(p.workload, out_fmt, ro));
    return 0;
}

int run_generate(const Args& a, const RenderOptions& ro) {
    GeneratorConfig cfg;
    if (a.has("count")) cfg.count = static_cast<size_t>(to_int_option(a, "count", "sample count"));
    if (a.has("seed")) cfg.seed = static_cast<std::uint64_t>(to_int_option(a, "seed", "RNG seed"));
    if (a.has("min-procs")) cfg.min_processes = static_cast<int>(to_int_option(a, "min-procs", "min processes"));
    if (a.has("max-procs")) cfg.max_processes = static_cast<int>(to_int_option(a, "max-procs", "max processes"));
    if (a.has("objective")) cfg.objective = util::lower(a.get("objective"));
    if (a.has("algos")) cfg.algorithms = split_algos(a.get("algos"));
    cfg.balance = a.flags.count("balance") > 0;
    if (cfg.count > 5000000) throw EngineError("USAGE_ERROR", "--count is limited to 5,000,000.");

    Dataset ds = generate_dataset(cfg);
    OutputFormat fmt = parse_output_format(a.get("format", "csv"));
    write_all(a.get("output"), render_dataset(ds, fmt, a.flags.count("with-metrics") > 0, ro));

    // Summary goes to stderr so stdout stays a clean data stream.
    std::cerr << "Generated " << ds.samples.size() << " samples (" << ds.attempts << " candidates, objective '"
              << cfg.objective << "'). Class distribution:\n";
    for (const auto& kv : ds.class_counts)
        std::cerr << "  " << kv.first << ": " << kv.second << " ("
                  << util::fmt_double(util::round4(100.0 * static_cast<double>(kv.second) / static_cast<double>(ds.samples.size())))
                  << "%)\n";
    return 0;
}

int run_algorithms() {
    Json arr = Json::array();
    for (const auto& a : all_algorithms()) {
        Json j = Json::object();
        j.set("id", a.id);
        j.set("label", a.label);
        j.set("core", a.core);
        arr.push(j);
    }
    Json root = Json::object();
    root.set("status", "ok");
    root.set("algorithms", arr);
    std::cout << root.dump() << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    OutputFormat out_fmt = OutputFormat::Json;
    try {
        Args a = parse_args(argc, argv);
        if (a.flags.count("version")) {
            std::cout << "schedsim " << kEngineVersion << "\n";
            return 0;
        }
        if (a.flags.count("help") || a.command.empty() || a.command == "help") {
            std::cout << kUsage;
            return a.command.empty() && !a.flags.count("help") ? 1 : 0;
        }

        out_fmt = parse_output_format(a.get("out-format", "json"));
        RenderOptions ro;
        ro.json_indent = a.flags.count("pretty") ? 2 : -1;
        ro.csv_view = a.get("view", "summary");

        if (a.command == "simulate") return run_simulate(a, out_fmt, ro);
        if (a.command == "features") return run_features(a, out_fmt, ro);
        if (a.command == "generate") return run_generate(a, ro);
        if (a.command == "algorithms") return run_algorithms();
        throw EngineError("USAGE_ERROR", "Unknown command '" + a.command + "'. Try --help.");
    } catch (const EngineError& e) {
        if (out_fmt == OutputFormat::Json) std::cout << error_to_json(e.code(), e.what()).dump() << "\n";
        else std::cerr << "error [" << e.code() << "]: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        if (out_fmt == OutputFormat::Json) std::cout << error_to_json("INTERNAL_ERROR", e.what()).dump() << "\n";
        else std::cerr << "error [INTERNAL_ERROR]: " << e.what() << "\n";
        return 1;
    }
}
