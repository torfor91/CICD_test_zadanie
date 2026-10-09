#include "json.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

namespace json {

// ---------- Токенизатор ----------
struct Token {
    enum Kind { LBrace, RBrace, LBracket, RBracket, Colon, Comma,
                String, Number, True, False, Null, End } kind;
    std::string text;
    double num = 0;
};

class Lexer {
public:
    explicit Lexer(const std::string& s) : src(s) {}
    Token next() {
        skip_ws();
        if (pos >= src.size()) return {Token::End, ""};
        char c = src[pos];
        switch (c) {
            case '{': ++pos; return {Token::LBrace, "{"};
            case '}': ++pos; return {Token::RBrace, "}"};
            case '[': ++pos; return {Token::LBracket, "["};
            case ']': ++pos; return {Token::RBracket, "]"};
            case ':': ++pos; return {Token::Colon, ":"};
            case ',': ++pos; return {Token::Comma, ","};
            case '"': return read_string();
            default:
                if (c == '-' || std::isdigit((unsigned char)c)) return read_number();
                if (src.compare(pos, 4, "true") == 0)  { pos += 4; return {Token::True, "true"}; }
                if (src.compare(pos, 5, "false") == 0) { pos += 5; return {Token::False, "false"}; }
                if (src.compare(pos, 4, "null") == 0)  { pos += 4; return {Token::Null, "null"}; }
                throw std::runtime_error(std::string("JSON: unexpected char '") + c + "'");
        }
    }
private:
    void skip_ws() { while (pos < src.size() && std::isspace((unsigned char)src[pos])) ++pos; }
    Token read_string() {
        ++pos; // skip "
        std::string out;
        while (pos < src.size() && src[pos] != '"') {
            char c = src[pos++];
            if (c == '\\' && pos < src.size()) {
                char e = src[pos++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case '"': out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/';  break;
                    default: out += e; break;
                }
            } else out += c;
        }
        if (pos >= src.size()) throw std::runtime_error("JSON: unterminated string");
        ++pos; // skip closing "
        return {Token::String, out};
    }
    Token read_number() {
        size_t start = pos;
        if (src[pos] == '-') ++pos;
        while (pos < src.size() && (std::isdigit((unsigned char)src[pos]) || src[pos] == '.' ||
                                    src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-'))
            ++pos;
        std::string t = src.substr(start, pos - start);
        Token tk{Token::Number, t};
        tk.num = std::stod(t);
        return tk;
    }
    const std::string& src;
    size_t pos = 0;
};

// ---------- Рекурсивный спуск ----------
class Parser {
public:
    explicit Parser(const std::string& s) : lex(s) { cur = lex.next(); }

    Value parse_value() {
        switch (cur.kind) {
            case Token::LBrace:   return parse_object();
            case Token::LBracket: return parse_array();
            case Token::String:   { Value v; v.type = Value::String; v.str = cur.text; advance(); return v; }
            case Token::Number:   { Value v; v.type = Value::Number; v.num = cur.num;  advance(); return v; }
            case Token::True:     { Value v; v.type = Value::Bool; v.b = true;  advance(); return v; }
            case Token::False:    { Value v; v.type = Value::Bool; v.b = false; advance(); return v; }
            case Token::Null:     { Value v; v.type = Value::Null; advance(); return v; }
            default: throw std::runtime_error("JSON: unexpected token");
        }
    }
private:
    void advance() { cur = lex.next(); }
    void expect(Token::Kind k) {
        if (cur.kind != k) throw std::runtime_error("JSON: syntax error");
        advance();
    }
    Value parse_object() {
        Value v; v.type = Value::Obj; v.obj = std::make_shared<Object>();
        expect(Token::LBrace);
        if (cur.kind == Token::RBrace) { advance(); return v; }
        while (true) {
            if (cur.kind != Token::String) throw std::runtime_error("JSON: expected key");
            std::string key = cur.text; advance();
            expect(Token::Colon);
            (*v.obj)[key] = parse_value();
            if (cur.kind == Token::Comma) { advance(); continue; }
            if (cur.kind == Token::RBrace) { advance(); break; }
            throw std::runtime_error("JSON: expected ',' or '}'");
        }
        return v;
    }
    Value parse_array() {
        Value v; v.type = Value::Arr; v.arr = std::make_shared<Array>();
        expect(Token::LBracket);
        if (cur.kind == Token::RBracket) { advance(); return v; }
        while (true) {
            v.arr->push_back(parse_value());
            if (cur.kind == Token::Comma) { advance(); continue; }
            if (cur.kind == Token::RBracket) { advance(); break; }
            throw std::runtime_error("JSON: expected ',' or ']'");
        }
        return v;
    }
    Lexer lex;
    Token cur;
};

// ---------- Публичные функции ----------
const Value& Value::operator[](const std::string& key) const {
    if (!obj) throw std::runtime_error("JSON: not an object");
    auto it = obj->find(key);
    if (it == obj->end()) throw std::runtime_error("JSON: no key '" + key + "'");
    return it->second;
}

const Value& Value::operator[](size_t idx) const {
    if (!arr || idx >= arr->size()) throw std::runtime_error("JSON: array index out of range");
    return (*arr)[idx];
}

Value parse(const std::string& text) {
    Parser p(text);
    return p.parse_value();
}

Value parse_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("JSON: cannot open " + path);
    std::stringstream ss; ss << f.rdbuf();
    return parse(ss.str());
}

} // namespace json