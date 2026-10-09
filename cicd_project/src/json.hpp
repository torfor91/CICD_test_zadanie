#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>

namespace json {

struct Value;
using Object = std::map<std::string, Value>;
using Array  = std::vector<Value>;

struct Value {
    enum Type { Null, Bool, Number, String, Arr, Obj } type = Null;
    bool        b = false;
    double      num = 0;
    std::string str;
    std::shared_ptr<Array>  arr;
    std::shared_ptr<Object> obj;

    bool is_object() const { return type == Obj; }
    bool is_array()  const { return type == Arr; }
    bool is_string() const { return type == String; }

    const Value& operator[](const std::string& key) const;
    const Value& operator[](size_t idx) const;
};

Value parse(const std::string& text);
Value parse_file(const std::string& path);

} // namespace json