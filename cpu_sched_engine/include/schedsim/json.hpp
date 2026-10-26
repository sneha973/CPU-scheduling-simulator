// Minimal dependency-free JSON value, parser and serializer.
#pragma once

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace schedsim {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool b) : type_(Type::Bool), bool_(b) {}
    template <typename T,
              typename = std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value>>
    Json(T v) : type_(Type::Number), num_(static_cast<double>(v)) {}
    Json(const char* s) : type_(Type::String), str_(s) {}
    Json(std::string s) : type_(Type::String), str_(std::move(s)) {}

    static Json array() { Json j; j.type_ = Type::Array; return j; }
    static Json object() { Json j; j.type_ = Type::Object; return j; }

    Type type() const { return type_; }
    bool is_null() const { return type_ == Type::Null; }
    bool is_bool() const { return type_ == Type::Bool; }
    bool is_number() const { return type_ == Type::Number; }
    bool is_string() const { return type_ == Type::String; }
    bool is_array() const { return type_ == Type::Array; }
    bool is_object() const { return type_ == Type::Object; }

    bool as_bool() const { return bool_; }
    double as_number() const { return num_; }
    const std::string& as_string() const { return str_; }
    const std::vector<Json>& items() const { return arr_; }
    const std::vector<std::pair<std::string, Json>>& members() const { return obj_; }

    // Builders. push() appends to an array, set() inserts/replaces in an object (order preserved).
    Json& push(Json v);
    Json& set(const std::string& key, Json v);

    // Lookup in an object; returns nullptr when absent or not an object.
    const Json* find(const std::string& key) const;

    // indent < 0 -> compact, otherwise pretty-printed with that many spaces.
    std::string dump(int indent = -1) const;

    // Throws EngineError("PARSE_ERROR", ...) on malformed input.
    static Json parse(const std::string& text);

private:
    void dump_to(std::string& out, int indent, int level) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Json> arr_;
    std::vector<std::pair<std::string, Json>> obj_;
};

}  // namespace schedsim
