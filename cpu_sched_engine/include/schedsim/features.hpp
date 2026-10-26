// Workload feature extraction: the numeric "fingerprint" the ML model is trained on.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "schedsim/types.hpp"

namespace schedsim {

// Fixed, ordered list of feature names. THIS ORDER IS PART OF THE ML CONTRACT:
// training CSV columns and prediction vectors both use it.
const std::vector<std::string>& feature_names();

// Values in the same order as feature_names().
std::vector<double> extract_features(const Workload& w);

// Convenience: (name, value) pairs.
std::vector<std::pair<std::string, double>> extract_named_features(const Workload& w);

}  // namespace schedsim
