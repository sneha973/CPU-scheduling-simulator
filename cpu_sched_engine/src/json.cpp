#include "schedsim/json.hpp"

#include <cstdio>
#include <cstdlib>

#include "schedsim/types.hpp"
#include "schedsim/util.hpp"

namespace schedsim {

Json& Json::push(Json v) {
    arr_.push_back(std::move(v));
    return *this;
}

Json& Json::set(const std::string& key, Json v) {
    for (auto& kv : obj_) {
        if (kv.first == key) {
            kv.second = std::move(v);
            return *this;
        }
    }
    obj_.emplace_back(key, std::move(v));
    return *this;
}

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& kv : obj_)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

// ---------------------------------------------------------------- serializer

static void escape_to(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

void Json::dump_to(std::string& out, int indent, int level) const {
    auto newline = [&](int lvl) {
        if (indent < 0) return;
        out.push_back('\n');
        out.append(static_cast<size_t>(indent * lvl), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: out += util::fmt_double(num_); break;
        case Type::String: escape_to(out, str_); break;
        case Type::Array:
            if (arr_.empty()) { out += "[]"; break; }
            out.push_back('[');
            for (size_t i = 0; i < arr_.size(); ++i) {
                if (i) out.push_back(',');
                newline(level + 1);
                arr_[i].dump_to(out, indent, level + 1);
            }
            newline(level);
            out.push_back(']');
            break;
        case Type::Object:
            if (obj_.empty()) { out += "{}"; break; }
            out.push_back('{');
            for (size_t i = 0; i < obj_.size(); ++i) {
                if (i) out.push_back(',');
                newline(level + 1);
                escape_to(out, obj_[i].first);
                out.push_back(':');
                if (indent >= 0) out.push_back(' ');
                obj_[i].second.dump_to(out, indent, level + 1);
            }
            newline(level);
            out.push_back('}');
            break;
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dump_to(out, indent, 0);
    return out;
}

// -------------------------------------------------------------------- parser

namespace {

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    Json parse_document() {
        skip_ws();
        Json v = parse_value();
        skip_ws();
        if (i_ != s_.size()) fail("unexpected trailing characters");
        return v;
    }

private:
    [[noreturn]] void fail(const std::string& msg) const {
        throw EngineError("PARSE_ERROR",
                          "Invalid JSON at offset " + std::to_string(i_) + ": " + msg);
    }

    void skip_ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }

    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }

    void expect(char c) {
        if (peek() != c) fail(std::string("expected '") + c + "'");
        ++i_;
    }

    bool match_word(const char* w) {
        size_t n = std::char_traits<char>::length(w);
        if (s_.compare(i_, n, w) == 0) { i_ += n; return true; }
        return false;
    }

    Json parse_value() {
        if (++depth_ > 64) fail("nesting too deep");
        Json result;
        char c = peek();
        if (c == '{') result = parse_object();
        else if (c == '[') result = parse_array();
        else if (c == '"') result = Json(parse_string());
        else if (c == '-' || (c >= '0' && c <= '9')) result = parse_number();
        else if (match_word("true")) result = Json(true);
        else if (match_word("false")) result = Json(false);
        else if (match_word("null")) result = Json();
        else fail("unexpected character");
        --depth_;
        return result;
    }

    Json parse_object() {
        expect('{');
        Json obj = Json::object();
        skip_ws();
        if (peek() == '}') { ++i_; return obj; }
        while (true) {
            skip_ws();
            if (peek() != '"') fail("object key must be a string");
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            obj.set(key, parse_value());
            skip_ws();
            if (peek() == ',') { ++i_; continue; }
            expect('}');
            return obj;
        }
    }

    Json parse_array() {
        expect('[');
        Json arr = Json::array();
        skip_ws();
        if (peek() == ']') { ++i_; return arr; }
        while (true) {
            skip_ws();
            arr.push(parse_value());
            skip_ws();
            if (peek() == ',') { ++i_; continue; }
            expect(']');
            return arr;
        }
    }

    unsigned parse_hex4() {
        if (i_ + 4 > s_.size()) fail("truncated \\u escape");
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s_[i_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("bad hex digit in \\u escape");
        }
        return v;
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (i_ >= s_.size()) fail("unterminated string");
            char c = s_[i_++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out.push_back(c); continue; }
            if (i_ >= s_.size()) fail("unterminated escape");
            char e = s_[i_++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    unsigned cp = parse_hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF && s_.compare(i_, 2, "\\u") == 0) {
                        i_ += 2;
                        unsigned lo = parse_hex4();
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: fail("invalid escape sequence");
            }
        }
    }

    Json parse_number() {
        size_t start = i_;
        if (peek() == '-') ++i_;
        if (!std::isdigit(static_cast<unsigned char>(peek()))) fail("invalid number");
        while (std::isdigit(static_cast<unsigned char>(peek()))) ++i_;
        if (peek() == '.') {
            ++i_;
            if (!std::isdigit(static_cast<unsigned char>(peek()))) fail("invalid number");
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++i_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++i_;
            if (peek() == '+' || peek() == '-') ++i_;
            if (!std::isdigit(static_cast<unsigned char>(peek()))) fail("invalid number");
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++i_;
        }
        return Json(std::strtod(s_.substr(start, i_ - start).c_str(), nullptr));
    }

    const std::string& s_;
    size_t i_ = 0;
    int depth_ = 0;
};

}  // namespace

Json Json::parse(const std::string& text) {
    Parser p(text);
    return p.parse_document();
}

}  // namespace schedsim
