// Output side of the engine: Report / Dataset -> JSON, CSV, text or XML.
#pragma once

#include <string>

#include "schedsim/generator.hpp"
#include "schedsim/json.hpp"
#include "schedsim/types.hpp"

namespace schedsim {

enum class OutputFormat { Json, Csv, Text, Xml };

// "json", "csv", "text"/"txt", "xml" (throws USAGE_ERROR otherwise)
OutputFormat parse_output_format(const std::string& name);

struct RenderOptions {
    int json_indent = -1;             // -1 compact, >=0 pretty
    std::string csv_view = "summary"; // summary | processes | gantt
};

// Structured form of a report. This is THE contract with the Django layer.
Json report_to_json(const Report& report);

// Standard failure reply: {"status":"error","code":...,"message":...}
Json error_to_json(const std::string& code, const std::string& message);

std::string render_report(const Report& report, OutputFormat format, const RenderOptions& opts);

// Feature vector of a workload (for the ML predictor at inference time).
Json features_to_json(const Workload& w);
std::string render_features(const Workload& w, OutputFormat format, const RenderOptions& opts);

// ML training data.
std::string render_dataset(const Dataset& ds, OutputFormat format, bool with_metrics, const RenderOptions& opts);

}  // namespace schedsim
