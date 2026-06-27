#include "cfgscript/cfgscript.h"

#include <cctype>
#include <cstdlib>

namespace helioforge::cfgscript {
namespace {

class Parser {
 public:
  Parser(const char* text, size_t size) : text_(text), size_(size) {}
  ParseResult parse_document() {
    ParseResult res;
    skip_ws();
    while (!eof()) {
      if (match_kw("include")) {
        IncludeRecord inc;
        if (!parse_include(&inc, &res)) break;
        res.value.includes.push_back(std::move(inc));
      } else {
        std::string key;
        if (!identifier(&key)) {
          error(&res, "expected identifier");
          break;
        }
        if (!consume('=')) {
          error(&res, "expected assignment");
          break;
        }
        Value value;
        if (!parse_value(&value, &res)) break;
        res.value.globals[key] = std::move(value);
        consume(';');
      }
      skip_ws();
    }
    collect_refs(res.value.globals, &res.value.symbol_refs);
    res.ok = res.errors.empty();
    return res;
  }

 private:
  bool eof() const { return pos_ >= size_; }
  char peek() const { return eof() ? '\0' : text_[pos_]; }
  char get() { return eof() ? '\0' : text_[pos_++]; }
  void skip_ws() {
    for (;;) {
      while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) ++pos_;
      if (peek() == '#') {
        while (!eof() && peek() != '\n') ++pos_;
      } else if (peek() == '/' && pos_ + 1 < size_ && text_[pos_ + 1] == '/') {
        pos_ += 2;
        while (!eof() && peek() != '\n') ++pos_;
      } else {
        break;
      }
    }
  }
  bool consume(char c) {
    skip_ws();
    if (peek() == c) {
      ++pos_;
      return true;
    }
    return false;
  }
  bool match_kw(const char* kw) {
    skip_ws();
    size_t start = pos_;
    for (size_t i = 0; kw[i]; ++i) {
      if (start + i >= size_ || text_[start + i] != kw[i]) return false;
    }
    size_t end = start;
    while (kw[end - start]) ++end;
    if (end < size_ && (std::isalnum(static_cast<unsigned char>(text_[end])) ||
                        text_[end] == '_')) {
      return false;
    }
    pos_ = end;
    return true;
  }
  bool identifier(std::string* out) {
    skip_ws();
    if (!(std::isalpha(static_cast<unsigned char>(peek())) || peek() == '_')) return false;
    size_t start = pos_++;
    while (!eof() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_' ||
                     peek() == '-' || peek() == '.')) {
      ++pos_;
    }
    out->assign(text_ + start, pos_ - start);
    return true;
  }
  bool quoted(std::string* out, ParseResult* res) {
    skip_ws();
    if (get() != '"') return false;
    std::string s;
    while (!eof()) {
      char c = get();
      if (c == '"') {
        *out = s;
        return true;
      }
      if (c == '\\') {
        if (eof()) break;
        char e = get();
        switch (e) {
          case 'n': s.push_back('\n'); break;
          case 't': s.push_back('\t'); break;
          case '"': s.push_back('"'); break;
          case '\\': s.push_back('\\'); break;
          default: s.push_back(e); break;
        }
      } else {
        s.push_back(c);
      }
    }
    error(res, "unterminated string");
    return false;
  }
  bool number(Value* v) {
    skip_ws();
    size_t start = pos_;
    if (peek() == '-') ++pos_;
    while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) ++pos_;
    if (pos_ == start || (pos_ == start + 1 && text_[start] == '-')) return false;
    v->data = static_cast<int64_t>(std::strtoll(std::string(text_ + start, pos_ - start).c_str(), nullptr, 10));
    return true;
  }
  bool parse_include(IncludeRecord* inc, ParseResult* res) {
    if (!quoted(&inc->name, res)) return false;
    if (consume('{')) {
      if (!parse_map_body(&inc->attributes, '}', res)) return false;
    }
    consume(';');
    return true;
  }
  bool parse_array(Value* v, ParseResult* res) {
    Array arr;
    if (!consume('[')) return false;
    while (!consume(']')) {
      Value item;
      if (!parse_value(&item, res)) return false;
      arr.push_back(std::move(item));
      if (!consume(',')) {
        if (!consume(']')) {
          error(res, "expected array delimiter");
          return false;
        }
        break;
      }
    }
    v->data = std::move(arr);
    return true;
  }
  bool parse_map(Value* v, ParseResult* res) {
    Map map;
    if (!consume('{')) return false;
    if (!parse_map_body(&map, '}', res)) return false;
    v->data = std::move(map);
    return true;
  }
  bool parse_map_body(Map* map, char end, ParseResult* res) {
    skip_ws();
    while (!consume(end)) {
      std::string key;
      if (!identifier(&key) && !quoted(&key, res)) {
        error(res, "expected map key");
        return false;
      }
      if (!consume(':') && !consume('=')) {
        error(res, "expected map separator");
        return false;
      }
      Value val;
      if (!parse_value(&val, res)) return false;
      (*map)[key] = std::move(val);
      consume(';');
      consume(',');
    }
    return true;
  }
  bool parse_expr(Value* v, ParseResult* res) {
    if (!consume('$')) return false;
    Expr expr;
    std::string name;
    if (!identifier(&name)) {
      error(res, "expected symbol");
      return false;
    }
    expr.op = "symbol";
    expr.atoms.push_back(name);
    if (consume('(')) {
      expr.op = name;
      expr.atoms.clear();
      while (!consume(')')) {
        std::string atom;
        if (quoted(&atom, res) || identifier(&atom)) {
          expr.atoms.push_back(atom);
        } else {
          Value n;
          if (number(&n)) expr.atoms.push_back("number");
          else {
            error(res, "bad expression atom");
            return false;
          }
        }
        consume(',');
      }
    }
    v->data = std::move(expr);
    return true;
  }
  bool parse_value(Value* v, ParseResult* res) {
    skip_ws();
    if (peek() == '"') {
      std::string s;
      if (!quoted(&s, res)) return false;
      v->data = std::move(s);
      return true;
    }
    if (peek() == '[') return parse_array(v, res);
    if (peek() == '{') return parse_map(v, res);
    if (peek() == '$') return parse_expr(v, res);
    if (match_kw("true")) {
      v->data = true;
      return true;
    }
    if (match_kw("false")) {
      v->data = false;
      return true;
    }
    return number(v) || bare_string(v);
  }
  bool bare_string(Value* v) {
    std::string id;
    if (!identifier(&id)) return false;
    v->data = std::move(id);
    return true;
  }
  static void collect_refs(const Map& map, std::vector<std::string>* refs) {
    for (const auto& kv : map) collect_value(kv.second, refs);
  }
  static void collect_value(const Value& v, std::vector<std::string>* refs) {
    if (auto e = std::get_if<Expr>(&v.data)) {
      if (e->op == "symbol" && !e->atoms.empty()) refs->push_back(e->atoms.front());
    } else if (auto a = std::get_if<Array>(&v.data)) {
      for (const auto& item : *a) collect_value(item, refs);
    } else if (auto m = std::get_if<Map>(&v.data)) {
      collect_refs(*m, refs);
    }
  }
  void error(ParseResult* res, const std::string& msg) {
    res->errors.push_back({msg, pos_});
  }

  const char* text_;
  size_t size_;
  size_t pos_ = 0;
};

}  // namespace

ParseResult parse(const char* data, size_t size) {
  Parser p(data, size);
  return p.parse_document();
}

ParseResult parse(const uint8_t* data, size_t size) {
  return parse(reinterpret_cast<const char*>(data), size);
}

}  // namespace helioforge::cfgscript
