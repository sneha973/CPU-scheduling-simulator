#include "schedsim/parsers.hpp"

#include <map>

#include "schedsim/json.hpp"
#include "schedsim/util.hpp"

namespace schedsim {

namespace {

using util::lower;
using util::normalize_key;
using util::trim;

// ------------------------------------------------------------- field handling

enum class Field { None, Pid, Arrival, Burst, Priority };
using FieldMap = std::map<Field, std::string>;

Field field_of(const std::string& key) {
    const std::string k = normalize_key(key);
    if (k == "pid" || k == "id" || k == "name" || k == "process" || k == "processid" || k == "processname" ||
        k == "p" || k == "job" || k == "jobid")
        return Field::Pid;
    if (k == "arrival" || k == "arrivaltime" || k == "at" || k == "arrive")
        return Field::Arrival;
    if (k == "burst" || k == "bursttime" || k == "bt" || k == "cpuburst" || k == "executiontime" || k == "duration")
        return Field::Burst;
    if (k == "priority" || k == "prio" || k == "pr" || k == "pri")
        return Field::Priority;
    return Field::None;
}

[[noreturn]] void bad(const std::string& msg) { throw EngineError("INVALID_INPUT", msg); }

Time need_time(const FieldMap& f, Field field, const char* label, const std::string& pid, bool required,
               Time fallback) {
    auto it = f.find(field);
    if (it == f.end() || trim(it->second).empty()) {
        if (required) bad("Process '" + pid + "': missing " + label + ".");
        return fallback;
    }
    auto v = util::parse_time(it->second);
    if (!v) bad("Process '" + pid + "': " + label + " must be an integer, got '" + trim(it->second) + "'.");
    return *v;
}

Process make_process(const FieldMap& f, size_t index) {
    Process p;
    auto it = f.find(Field::Pid);
    p.pid = (it != f.end() && !trim(it->second).empty()) ? trim(it->second) : "P" + std::to_string(index + 1);
    p.arrival = need_time(f, Field::Arrival, "arrival time", p.pid, false, 0);
    p.burst = need_time(f, Field::Burst, "burst time", p.pid, true, 0);
    p.priority = need_time(f, Field::Priority, "priority", p.pid, false, 0);
    return p;
}

// ------------------------------------------------------------- shared config keys

std::vector<std::string> split_list(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '|') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

// Returns true when `key` was a recognised setting.
bool apply_config(Request& req, const std::string& key, const std::string& value) {
    const std::string k = normalize_key(key);
    if (k == "timequantum" || k == "quantum" || k == "tq") {
        auto v = util::parse_time(value);
        if (!v) bad("time_quantum must be an integer, got '" + trim(value) + "'.");
        req.workload.time_quantum = *v;
        return true;
    }
    if (k == "algorithms" || k == "algorithm" || k == "algos") {
        req.algorithms = split_list(value);
        return true;
    }
    if (k == "objective") {
        req.objective = lower(trim(value));
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------ JSON

std::string json_scalar_to_string(const Json& j) {
    if (j.is_string()) return j.as_string();
    if (j.is_number()) return util::fmt_double(j.as_number());  // 2.5 stays "2.5" -> rejected later
    if (j.is_bool()) return j.as_bool() ? "1" : "0";
    return "";
}

Request parse_json_request(const std::string& text) {
    Json root = Json::parse(text);
    Request req;

    const Json* procs = nullptr;
    if (root.is_array()) {
        procs = &root;
    } else if (root.is_object()) {
        for (const char* key : {"processes", "jobs", "tasks"})
            if ((procs = root.find(key))) break;
    }
    if (!procs || !procs->is_array())
        bad("JSON input needs a \"processes\" array (or a top-level array of processes).");

    size_t index = 0;
    for (const Json& item : procs->items()) {
        if (!item.is_object()) bad("Every entry in \"processes\" must be an object.");
        FieldMap f;
        for (const auto& kv : item.members()) {
            Field fld = field_of(kv.first);
            if (fld != Field::None) f[fld] = json_scalar_to_string(kv.second);
        }
        req.workload.processes.push_back(make_process(f, index++));
    }

    if (root.is_object()) {
        auto scan = [&](const Json& obj) {
            for (const auto& kv : obj.members()) {
                if (kv.first == "processes" || kv.first == "jobs" || kv.first == "tasks") continue;
                if (kv.second.is_array() && normalize_key(kv.first).rfind("algo", 0) == 0) {
                    std::vector<std::string> ids;
                    for (const Json& a : kv.second.items()) ids.push_back(json_scalar_to_string(a));
                    req.algorithms = ids;
                } else if (!kv.second.is_object() && !kv.second.is_array()) {
                    apply_config(req, kv.first, json_scalar_to_string(kv.second));
                }
            }
        };
        scan(root);
        for (const char* nested : {"config", "options", "settings"})
            if (const Json* sub = root.find(nested))
                if (sub->is_object()) scan(*sub);
    }
    return req;
}

// ------------------------------------------------------------ CSV and plain text

std::vector<std::string> split_cells(const std::string& line, bool whitespace, char delim) {
    std::vector<std::string> cells;
    if (whitespace) {
        std::string cur;
        for (char c : line) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!cur.empty()) cells.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) cells.push_back(cur);
    } else {
        for (auto& c : util::split(line, delim)) cells.push_back(trim(c));
    }
    for (auto& c : cells)
        if (c.size() >= 2 && ((c.front() == '"' && c.back() == '"') || (c.front() == '\'' && c.back() == '\'')))
            c = c.substr(1, c.size() - 2);
    return cells;
}

Request parse_delimited_request(const std::string& text, bool whitespace) {
    Request req;
    std::string content = text;
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF)
        content.erase(0, 3);  // UTF-8 BOM (Excel likes to add it)

    char delim = ',';
    bool delim_chosen = false;
    bool header_seen = false;
    std::vector<Field> columns;
    size_t index = 0;
    size_t line_no = 0;

    for (const std::string& raw : util::split(content, '\n')) {
        ++line_no;
        std::string line = trim(raw);
        if (line.empty()) continue;

        if (line[0] == '#') {  // "# quantum=2 objective=wt" style settings inside comments
            for (const auto& tok : split_list(line.substr(1))) {
                auto eq = tok.find('=');
                if (eq != std::string::npos) apply_config(req, tok.substr(0, eq), tok.substr(eq + 1));
            }
            continue;
        }

        // "key = value" / "key: value" settings lines
        auto sep = line.find_first_of("=:");
        if (sep != std::string::npos) {
            std::string k = trim(line.substr(0, sep)), v = trim(line.substr(sep + 1));
            if (apply_config(req, k, v)) continue;
        }

        if (!whitespace && !delim_chosen) {
            if (line.find('\t') != std::string::npos) delim = '\t';
            else if (line.find(';') != std::string::npos && line.find(',') == std::string::npos) delim = ';';
            delim_chosen = true;
        }
        std::vector<std::string> cells = split_cells(line, whitespace, delim);
        if (cells.empty()) continue;

        // "quantum 2" / "quantum,2" settings lines
        if (cells.size() == 2 && field_of(cells[0]) == Field::None && apply_config(req, cells[0], cells[1])) continue;

        if (!header_seen) {
            header_seen = true;
            bool looks_like_header = false;
            for (const auto& c : cells)
                if (field_of(c) != Field::None && !util::parse_time(c)) looks_like_header = true;
            if (looks_like_header) {
                for (const auto& c : cells) columns.push_back(field_of(c));
                bool has_burst = false;
                for (Field f : columns) has_burst |= (f == Field::Burst);
                if (!has_burst) bad("Header (line " + std::to_string(line_no) + ") has no burst-time column.");
                continue;
            }
            if (cells.size() >= 4) columns = {Field::Pid, Field::Arrival, Field::Burst, Field::Priority};
            else if (cells.size() == 3) columns = {Field::Pid, Field::Arrival, Field::Burst};
            else bad("Line " + std::to_string(line_no) + ": expected 'pid arrival burst [priority]'.");
        }

        if (cells.size() < columns.size())
            bad("Line " + std::to_string(line_no) + ": expected " + std::to_string(columns.size()) +
                " values but found " + std::to_string(cells.size()) + ".");
        FieldMap f;
        for (size_t c = 0; c < columns.size(); ++c)
            if (columns[c] != Field::None) f[columns[c]] = cells[c];
        req.workload.processes.push_back(make_process(f, index++));
    }
    return req;
}

// ------------------------------------------------------------------------- XML

struct XmlNode {
    std::string name;
    std::map<std::string, std::string> attrs;
    std::vector<XmlNode> children;
    std::string text;
};

class XmlParser {
public:
    explicit XmlParser(const std::string& s) : s_(s) {}

    XmlNode parse_document() {
        skip_misc();
        if (peek() != '<') fail("document must start with an element");
        XmlNode root = parse_element();
        skip_misc();
        if (i_ != s_.size()) fail("unexpected content after the root element");
        return root;
    }

private:
    [[noreturn]] void fail(const std::string& msg) const {
        throw EngineError("PARSE_ERROR", "Invalid XML at offset " + std::to_string(i_) + ": " + msg);
    }
    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
    bool starts(const char* w) const { return s_.compare(i_, std::char_traits<char>::length(w), w) == 0; }
    void skip_ws() { while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_; }

    void skip_until(const char* end) {
        size_t pos = s_.find(end, i_);
        if (pos == std::string::npos) fail(std::string("missing '") + end + "'");
        i_ = pos + std::char_traits<char>::length(end);
    }

    // whitespace, <?xml ?>, comments, <!DOCTYPE ...>
    void skip_misc() {
        while (true) {
            skip_ws();
            if (starts("<?")) skip_until("?>");
            else if (starts("<!--")) skip_until("-->");
            else if (starts("<!")) skip_until(">");
            else return;
        }
    }

    static std::string decode(const std::string& s) {
        std::string out;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '&') {
                size_t semi = s.find(';', i);
                if (semi != std::string::npos) {
                    std::string ent = s.substr(i + 1, semi - i - 1);
                    if (ent == "amp") { out.push_back('&'); i = semi; continue; }
                    if (ent == "lt") { out.push_back('<'); i = semi; continue; }
                    if (ent == "gt") { out.push_back('>'); i = semi; continue; }
                    if (ent == "quot") { out.push_back('"'); i = semi; continue; }
                    if (ent == "apos") { out.push_back('\''); i = semi; continue; }
                }
            }
            out.push_back(s[i]);
        }
        return out;
    }

    std::string parse_name() {
        size_t start = i_;
        while (i_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[i_])) || s_[i_] == '_' ||
                                  s_[i_] == '-' || s_[i_] == ':' || s_[i_] == '.'))
            ++i_;
        if (i_ == start) fail("expected a name");
        return s_.substr(start, i_ - start);
    }

    XmlNode parse_element() {
        if (++depth_ > 64) fail("nesting too deep");
        if (peek() != '<') fail("expected '<'");
        ++i_;
        XmlNode node;
        node.name = parse_name();

        while (true) {  // attributes
            skip_ws();
            char c = peek();
            if (c == '/' || c == '>') break;
            std::string key = parse_name();
            skip_ws();
            if (peek() != '=') fail("expected '=' after attribute name");
            ++i_;
            skip_ws();
            char q = peek();
            if (q != '"' && q != '\'') fail("attribute value must be quoted");
            ++i_;
            size_t end = s_.find(q, i_);
            if (end == std::string::npos) fail("unterminated attribute value");
            node.attrs[key] = decode(s_.substr(i_, end - i_));
            i_ = end + 1;
        }

        if (peek() == '/') {
            ++i_;
            if (peek() != '>') fail("expected '>'");
            ++i_;
            --depth_;
            return node;
        }
        ++i_;  // '>'

        while (true) {  // content
            if (i_ >= s_.size()) fail("unexpected end of document inside <" + node.name + ">");
            if (starts("</")) {
                i_ += 2;
                std::string closing = parse_name();
                if (closing != node.name) fail("mismatched closing tag </" + closing + "> for <" + node.name + ">");
                skip_ws();
                if (peek() != '>') fail("expected '>'");
                ++i_;
                break;
            }
            if (starts("<!--")) { skip_until("-->"); continue; }
            if (starts("<![CDATA[")) {
                size_t end = s_.find("]]>", i_);
                if (end == std::string::npos) fail("unterminated CDATA");
                node.text += s_.substr(i_ + 9, end - i_ - 9);
                i_ = end + 3;
                continue;
            }
            if (peek() == '<') {
                node.children.push_back(parse_element());
            } else {
                size_t end = s_.find('<', i_);
                if (end == std::string::npos) fail("unexpected end of document");
                node.text += decode(s_.substr(i_, end - i_));
                i_ = end;
            }
        }
        --depth_;
        return node;
    }

    const std::string& s_;
    size_t i_ = 0;
    int depth_ = 0;
};

void xml_collect_processes(const XmlNode& node, Request& req, size_t& index) {
    for (const XmlNode& child : node.children) {
        const std::string name = normalize_key(child.name);
        if (name == "process" || name == "job" || name == "task") {
            FieldMap f;
            for (const auto& a : child.attrs)
                if (field_of(a.first) != Field::None) f[field_of(a.first)] = a.second;
            for (const XmlNode& sub : child.children)
                if (field_of(sub.name) != Field::None) f[field_of(sub.name)] = trim(sub.text);
            req.workload.processes.push_back(make_process(f, index++));
        } else if (name == "processes" || name == "jobs" || name == "tasks") {
            xml_collect_processes(child, req, index);
        } else if (name == "algorithms") {
            std::vector<std::string> ids;
            for (const XmlNode& a : child.children) ids.push_back(trim(a.text));
            if (ids.empty()) ids = split_list(child.text);
            req.algorithms = ids;
        } else {
            apply_config(req, child.name, trim(child.text));
        }
    }
}

Request parse_xml_request(const std::string& text) {
    XmlParser parser(text);
    XmlNode root = parser.parse_document();
    Request req;
    for (const auto& a : root.attrs) apply_config(req, a.first, a.second);
    size_t index = 0;
    xml_collect_processes(root, req, index);
    return req;
}

}  // namespace

// ------------------------------------------------------------------ public API

InputFormat parse_input_format(const std::string& name) {
    const std::string n = lower(trim(name));
    if (n == "auto" || n.empty()) return InputFormat::Auto;
    if (n == "json") return InputFormat::Json;
    if (n == "csv") return InputFormat::Csv;
    if (n == "txt" || n == "text") return InputFormat::Txt;
    if (n == "xml") return InputFormat::Xml;
    throw EngineError("USAGE_ERROR", "Unknown input format '" + name + "'. Valid: auto, json, csv, txt, xml");
}

InputFormat detect_input_format(const std::string& content, const std::string& filename_hint) {
    const std::string name = lower(filename_hint);
    auto ends = [&](const char* ext) {
        size_t n = std::char_traits<char>::length(ext);
        return name.size() >= n && name.compare(name.size() - n, n, ext) == 0;
    };
    if (ends(".json")) return InputFormat::Json;
    if (ends(".csv")) return InputFormat::Csv;
    if (ends(".xml")) return InputFormat::Xml;
    if (ends(".txt")) return InputFormat::Txt;

    size_t i = 0;
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF) i = 3;  // BOM
    while (i < content.size() && std::isspace(static_cast<unsigned char>(content[i]))) ++i;
    if (i >= content.size()) throw EngineError("PARSE_ERROR", "The input is empty.");
    if (content[i] == '{' || content[i] == '[') return InputFormat::Json;
    if (content[i] == '<') return InputFormat::Xml;
    return content.find(',') != std::string::npos || content.find(';') != std::string::npos ? InputFormat::Csv
                                                                                            : InputFormat::Txt;
}

Request parse_request(const std::string& content, InputFormat format, const std::string& filename_hint) {
    if (trim(content).empty()) throw EngineError("PARSE_ERROR", "The input is empty.");
    if (format == InputFormat::Auto) format = detect_input_format(content, filename_hint);
    switch (format) {
        case InputFormat::Json: return parse_json_request(content);
        case InputFormat::Csv: return parse_delimited_request(content, false);
        case InputFormat::Txt: return parse_delimited_request(content, true);
        case InputFormat::Xml: return parse_xml_request(content);
        default: break;
    }
    throw EngineError("USAGE_ERROR", "Unsupported input format.");
}

}  // namespace schedsim
