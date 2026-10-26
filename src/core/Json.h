#pragma once
// JSON mínimo (sin dependencias) para el formato de proyecto.
// Soporta el estándar completo salvo: números fuera del rango de double y
// pares sustitutos \uD800-\uDFFF (se rechazan con error explícito).
#include <string>
#include <utility>
#include <vector>

namespace nexo::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    static Value boolean(bool b) { Value v; v.type_ = Type::Bool; v.bool_ = b; return v; }
    static Value number(double d) { Value v; v.type_ = Type::Number; v.num_ = d; return v; }
    static Value string(std::string s) { Value v; v.type_ = Type::String; v.str_ = std::move(s); return v; }
    static Value array() { Value v; v.type_ = Type::Array; return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    double asNumber() const { return num_; }
    bool asBool() const { return bool_; }
    const std::string& asString() const { return str_; }
    const std::vector<Value>& items() const { return items_; }

    void push(Value v) { items_.push_back(std::move(v)); }
    void set(const std::string& key, Value v);
    // nullptr si no existe o no es un objeto.
    const Value* get(const std::string& key) const;

    // indent < 0: compacto; >= 0: legible con esa sangría por nivel.
    std::string dump(int indent = 2) const;

private:
    void dumpTo(std::string& out, int indent, int level) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double num_ = 0;
    std::string str_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;   // conserva el orden
};

// Devuelve false y rellena `error` (con línea y columna) si el texto no es JSON válido.
bool parse(const std::string& text, Value& out, std::string& error);

}  // namespace nexo::json
