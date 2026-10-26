#include "core/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace nexo::json {

void Value::set(const std::string& key, Value v) {
    type_ = Type::Object;
    for (auto& m : members_)
        if (m.first == key) { m.second = std::move(v); return; }
    members_.emplace_back(key, std::move(v));
}

const Value* Value::get(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& m : members_)
        if (m.first == key) return &m.second;
    return nullptr;
}

static void escapeTo(std::string& out, const std::string& s) {
    out += '"';
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
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void Value::dumpTo(std::string& out, int indent, int level) const {
    auto nl = [&](int lvl) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<std::size_t>(indent * lvl), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: {
            char buf[40];
            if (std::floor(num_) == num_ && std::fabs(num_) < 9e15)
                std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(num_));
            else
                std::snprintf(buf, sizeof buf, "%.17g", num_);
            out += buf;
            break;
        }
        case Type::String: escapeTo(out, str_); break;
        case Type::Array:
            if (items_.empty()) { out += "[]"; break; }
            out += '[';
            for (std::size_t i = 0; i < items_.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                items_[i].dumpTo(out, indent, level + 1);
            }
            nl(level);
            out += ']';
            break;
        case Type::Object:
            if (members_.empty()) { out += "{}"; break; }
            out += '{';
            for (std::size_t i = 0; i < members_.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                escapeTo(out, members_[i].first);
                out += indent < 0 ? ":" : ": ";
                members_[i].second.dumpTo(out, indent, level + 1);
            }
            nl(level);
            out += '}';
            break;
    }
}

std::string Value::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& t) : s_(t) {}

    bool run(Value& out, std::string& error) {
        skipWs();
        if (!value(out, 0)) { error = err_; return false; }
        skipWs();
        if (pos_ != s_.size()) { fail("Texto sobrante tras el valor JSON."); error = err_; return false; }
        return true;
    }

private:
    static constexpr int kMaxDepth = 64;

    bool fail(const std::string& msg) {
        int line = 1, col = 1;
        for (std::size_t i = 0; i < pos_ && i < s_.size(); ++i) {
            if (s_[i] == '\n') { ++line; col = 1; } else { ++col; }
        }
        err_ = "JSON inválido (línea " + std::to_string(line) + ", columna " + std::to_string(col) + "): " + msg;
        return false;
    }
    void skipWs() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }
    bool lit(const char* word) {
        std::size_t n = 0;
        while (word[n]) ++n;
        if (s_.compare(pos_, n, word) != 0) return false;
        pos_ += n;
        return true;
    }

    bool value(Value& out, int depth) {
        if (depth > kMaxDepth) return fail("Anidamiento demasiado profundo.");
        if (pos_ >= s_.size()) return fail("Fin de texto inesperado.");
        const char c = s_[pos_];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') { std::string str; if (!string(str)) return false; out = Value::string(std::move(str)); return true; }
        if (c == 't') { if (!lit("true")) return fail("Valor desconocido."); out = Value::boolean(true); return true; }
        if (c == 'f') { if (!lit("false")) return fail("Valor desconocido."); out = Value::boolean(false); return true; }
        if (c == 'n') { if (!lit("null")) return fail("Valor desconocido."); out = Value(); return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return number(out);
        return fail(std::string("Carácter inesperado '") + c + "'.");
    }

    bool number(Value& out) {
        const std::size_t start = pos_;
        if (s_[pos_] == '-') ++pos_;
        if (pos_ >= s_.size() || !(s_[pos_] >= '0' && s_[pos_] <= '9')) return fail("Número inválido.");
        if (s_[pos_] == '0') { ++pos_; }
        else while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        if (pos_ < s_.size() && s_[pos_] == '.') {
            ++pos_;
            if (pos_ >= s_.size() || !(s_[pos_] >= '0' && s_[pos_] <= '9')) return fail("Número inválido.");
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ >= s_.size() || !(s_[pos_] >= '0' && s_[pos_] <= '9')) return fail("Número inválido.");
            while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
        }
        const double d = std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr);
        if (!std::isfinite(d)) return fail("Número fuera de rango.");
        out = Value::number(d);
        return true;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) { out += static_cast<char>(cp); }
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    bool string(std::string& out) {
        ++pos_;  // comilla inicial
        while (pos_ < s_.size()) {
            const unsigned char c = static_cast<unsigned char>(s_[pos_]);
            if (c == '"') { ++pos_; return true; }
            if (c < 0x20) return fail("Carácter de control sin escapar en una cadena.");
            if (c != '\\') { out += static_cast<char>(c); ++pos_; continue; }
            ++pos_;
            if (pos_ >= s_.size()) break;
            const char e = s_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    if (pos_ + 4 > s_.size()) return fail("Escape \\u incompleto.");
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = s_[pos_++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else return fail("Escape \\u inválido.");
                    }
                    if (cp >= 0xD800 && cp <= 0xDFFF) return fail("Pares sustitutos \\u no soportados.");
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("Secuencia de escape desconocida.");
            }
        }
        return fail("Cadena sin cerrar.");
    }

    bool array(Value& out, int depth) {
        ++pos_;
        out = Value::array();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
        for (;;) {
            skipWs();
            Value item;
            if (!value(item, depth + 1)) return false;
            out.push(std::move(item));
            skipWs();
            if (pos_ >= s_.size()) return fail("Matriz sin cerrar.");
            if (s_[pos_] == ',') { ++pos_; continue; }
            if (s_[pos_] == ']') { ++pos_; return true; }
            return fail("Se esperaba ',' o ']'.");
        }
    }

    bool object(Value& out, int depth) {
        ++pos_;
        out = Value::object();
        skipWs();
        if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
        for (;;) {
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != '"') return fail("Se esperaba el nombre de una clave.");
            std::string key;
            if (!string(key)) return false;
            skipWs();
            if (pos_ >= s_.size() || s_[pos_] != ':') return fail("Se esperaba ':'.");
            ++pos_;
            skipWs();
            Value v;
            if (!value(v, depth + 1)) return false;
            out.set(key, std::move(v));
            skipWs();
            if (pos_ >= s_.size()) return fail("Objeto sin cerrar.");
            if (s_[pos_] == ',') { ++pos_; continue; }
            if (s_[pos_] == '}') { ++pos_; return true; }
            return fail("Se esperaba ',' o '}'.");
        }
    }

    const std::string& s_;
    std::size_t pos_ = 0;
    std::string err_;
};

}  // namespace

bool parse(const std::string& text, Value& out, std::string& error) {
    Parser p(text);
    return p.run(out, error);
}

}  // namespace nexo::json
