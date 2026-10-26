// Input side of the engine: turns JSON / CSV / TXT / XML text into a Request.
#pragma once

#include <string>

#include "schedsim/types.hpp"

namespace schedsim {

enum class InputFormat { Auto, Json, Csv, Txt, Xml };

// "auto", "json", "csv", "txt", "xml" (throws USAGE_ERROR otherwise)
InputFormat parse_input_format(const std::string& name);

// Looks at the file extension first (if given), then at the content.
InputFormat detect_input_format(const std::string& content, const std::string& filename_hint);

// Parses (and structurally checks) a workload. Semantic validation (ranges, duplicates)
// happens later in validate_workload().
Request parse_request(const std::string& content, InputFormat format, const std::string& filename_hint = "");

}  // namespace schedsim
