// Small string helpers (header-only).
#pragma once

#include <cctype>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "schedsim/types.hpp"

namespace schedsim {
namespace util {

inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

inline std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == delim) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

// "Arrival Time", "arrival_time", "ARRIVALTIME" -> "arrivaltime"
inline std::string normalize_key(const std::string& k) {
    std::string out;
    for (char c : k)
        if (std::isalnum(static_cast<unsigned char>(c)))
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

// Strict integer parse. Accepts "5", "-3", "+7" and "5.0"; rejects "5.5", "abc", "".
inline std::optional<Time> parse_time(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty()) return std::nullopt;
    size_t i = 0;
    bool neg = false;
    if (s[i] == '+' || s[i] == '-') {
        neg = (s[i] == '-');
        ++i;
    }
    size_t digits_start = i;
    Time v = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        if (i - digits_start > 17) return std::nullopt;
        v = v * 10 + (s[i] - '0');
        ++i;
    }
    if (i == digits_start) return std::nullopt;
    if (i < s.size()) {
        if (s[i] != '.') return std::nullopt;
        ++i;
        while (i < s.size()) {
            if (s[i] != '0') return std::nullopt;
            ++i;
        }
    }
    return neg ? -v : v;
}

// Integers print without decimals, everything else with up to 10 significant digits.
inline std::string fmt_double(double v) {
    if (!std::isfinite(v)) return "null";
    if (std::floor(v) == v && std::fabs(v) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(v));
        return buf;
    }
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

inline double round4(double v) { return std::round(v * 10000.0) / 10000.0; }

}  // namespace util
}  // namespace schedsim
