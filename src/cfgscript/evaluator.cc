#include "cfgscript/evaluator.h"

#include <algorithm>
#include <sstream>

namespace helioforge::cfgscript {
namespace {

constexpr size_t kMaxResolutionDepth = 128;
constexpr size_t kMaxStringifyDepth = 128;

struct ResolveContext {
  size_t depth = 0;
  std::vector<std::string> active_symbols;
};

Value clone_or_resolve(const Value& value, const Environment& env, const std::string& path,
                       Evaluation* eval, ResolveContext* ctx);

std::string join_path(const std::string& base, const std::string& name) {
  return base.empty() ? name : base + "." + name;
}

void flatten_impl(const Map& map, const std::string& prefix, std::vector<std::string>* out) {
  for (const auto& kv : map) {
    std::string path = join_path(prefix, kv.first);
    out->push_back(path);
    if (const auto* child = std::get_if<Map>(&kv.second.data)) flatten_impl(*child, path, out);
  }
}

std::string expr_to_string(const Expr& expr) {
  std::ostringstream out;
  out << "$" << expr.op;
  if (expr.op != "symbol") {
    out << "(";
    for (size_t i = 0; i < expr.atoms.size(); ++i) {
      if (i) out << ",";
      out << expr.atoms[i];
    }
    out << ")";
  } else if (!expr.atoms.empty()) {
    out << ":" << expr.atoms.front();
  }
  return out.str();
}

std::string value_to_string_limited(const Value& value, size_t depth) {
  if (depth > kMaxStringifyDepth) return "<max-depth>";
  if (std::holds_alternative<std::monostate>(value.data)) return "null";
  if (const auto* n = std::get_if<int64_t>(&value.data)) return std::to_string(*n);
  if (const auto* b = std::get_if<bool>(&value.data)) return *b ? "true" : "false";
  if (const auto* s = std::get_if<std::string>(&value.data)) return *s;
  if (const auto* expr = std::get_if<Expr>(&value.data)) return expr_to_string(*expr);
  if (const auto* arr = std::get_if<Array>(&value.data)) {
    std::string out = "[";
    for (size_t i = 0; i < arr->size(); ++i) {
      if (i) out += ",";
      out += value_to_string_limited((*arr)[i], depth + 1);
    }
    out += "]";
    return out;
  }
  const auto* map = std::get_if<Map>(&value.data);
  std::string out = "{";
  if (map) {
    bool first = true;
    for (const auto& kv : *map) {
      if (!first) out += ",";
      first = false;
      out += kv.first + ":" + value_to_string_limited(kv.second, depth + 1);
    }
  }
  out += "}";
  return out;
}

class DepthScope {
 public:
  explicit DepthScope(ResolveContext* ctx) : ctx_(ctx) { ++ctx_->depth; }
  ~DepthScope() { --ctx_->depth; }
  bool exceeded() const { return ctx_->depth > kMaxResolutionDepth; }

 private:
  ResolveContext* ctx_;
};

Value resolve_expr(const Expr& expr, const Environment& env, const std::string& path,
                   Evaluation* eval, ResolveContext* ctx) {
  DepthScope depth(ctx);
  if (depth.exceeded()) {
    eval->issues.push_back({path, "expression resolution depth exceeded"});
    Value fallback;
    fallback.data = expr_to_string(expr);
    return fallback;
  }
  if (expr.op == "symbol") {
    if (expr.atoms.empty()) {
      eval->issues.push_back({path, "empty symbol expression"});
      return {};
    }
    const std::string& symbol = expr.atoms.front();
    eval->referenced_symbols.insert(symbol);
    if (std::find(ctx->active_symbols.begin(), ctx->active_symbols.end(), symbol) !=
        ctx->active_symbols.end()) {
      eval->issues.push_back({path, "cyclic symbol reference " + symbol});
      Value fallback;
      fallback.data = expr_to_string(expr);
      return fallback;
    }
    const Value* found = env.get(symbol);
    if (!found) {
      eval->issues.push_back({path, "unresolved symbol " + symbol});
      Value fallback;
      fallback.data = expr_to_string(expr);
      return fallback;
    }
    ctx->active_symbols.push_back(symbol);
    Value resolved = clone_or_resolve(*found, env, path, eval, ctx);
    ctx->active_symbols.pop_back();
    return resolved;
  }
  if (expr.op == "concat") {
    std::string merged;
    for (const auto& atom : expr.atoms) {
      const Value* found = env.get(atom);
      if (found) merged += value_to_string_limited(*found, 0);
      else merged += atom;
    }
    Value out;
    out.data = std::move(merged);
    return out;
  }
  Value text;
  text.data = expr_to_string(expr);
  return text;
}

Value clone_or_resolve(const Value& value, const Environment& env, const std::string& path,
                       Evaluation* eval, ResolveContext* ctx) {
  if (ctx->depth > kMaxResolutionDepth) {
    eval->issues.push_back({path, "value resolution depth exceeded"});
    return value;
  }
  if (const auto* expr = std::get_if<Expr>(&value.data)) return resolve_expr(*expr, env, path, eval, ctx);
  if (const auto* arr = std::get_if<Array>(&value.data)) {
    Array copy;
    for (size_t i = 0; i < arr->size(); ++i) {
      DepthScope depth(ctx);
      if (depth.exceeded()) {
        eval->issues.push_back({path, "array resolution depth exceeded"});
        break;
      }
      copy.push_back(clone_or_resolve((*arr)[i], env, path + "[" + std::to_string(i) + "]", eval, ctx));
    }
    Value out;
    out.data = std::move(copy);
    return out;
  }
  if (const auto* map = std::get_if<Map>(&value.data)) {
    Map copy;
    for (const auto& kv : *map) {
      DepthScope depth(ctx);
      if (depth.exceeded()) {
        eval->issues.push_back({path, "map resolution depth exceeded"});
        break;
      }
      copy[kv.first] = clone_or_resolve(kv.second, env, join_path(path, kv.first), eval, ctx);
    }
    Value out;
    out.data = std::move(copy);
    return out;
  }
  return value;
}

}  // namespace

void Environment::set(std::string name, Value value) {
  symbols_[std::move(name)] = std::move(value);
}

bool Environment::has(const std::string& name) const {
  return symbols_.find(name) != symbols_.end();
}

const Value* Environment::get(const std::string& name) const {
  auto it = symbols_.find(name);
  return it == symbols_.end() ? nullptr : &it->second;
}

Evaluation Environment::evaluate(const Document& doc) const {
  Evaluation eval;
  Environment layered = *this;
  for (const auto& kv : doc.globals) layered.set(kv.first, kv.second);
  for (const auto& inc : doc.includes) eval.include_names.insert(inc.name);
  for (const auto& kv : doc.globals) {
    ResolveContext ctx;
    eval.resolved[kv.first] = clone_or_resolve(kv.second, layered, kv.first, &eval, &ctx);
  }
  return eval;
}

Evaluation evaluate_document(const Document& doc) {
  Environment env;
  return env.evaluate(doc);
}

std::vector<std::string> flatten_paths(const Map& map) {
  std::vector<std::string> out;
  flatten_impl(map, "", &out);
  std::sort(out.begin(), out.end());
  return out;
}

std::string value_to_string(const Value& value) {
  return value_to_string_limited(value, 0);
}

bool structurally_equal(const Value& a, const Value& b) {
  if (a.data.index() != b.data.index()) return false;
  if (std::holds_alternative<std::monostate>(a.data)) return true;
  if (const auto* av = std::get_if<int64_t>(&a.data)) return *av == std::get<int64_t>(b.data);
  if (const auto* av = std::get_if<bool>(&a.data)) return *av == std::get<bool>(b.data);
  if (const auto* av = std::get_if<std::string>(&a.data)) return *av == std::get<std::string>(b.data);
  if (const auto* ae = std::get_if<Expr>(&a.data)) {
    const auto& be = std::get<Expr>(b.data);
    return ae->op == be.op && ae->atoms == be.atoms;
  }
  if (const auto* aa = std::get_if<Array>(&a.data)) {
    const auto& ba = std::get<Array>(b.data);
    if (aa->size() != ba.size()) return false;
    for (size_t i = 0; i < aa->size(); ++i) {
      if (!structurally_equal((*aa)[i], ba[i])) return false;
    }
    return true;
  }
  const auto& am = std::get<Map>(a.data);
  const auto& bm = std::get<Map>(b.data);
  if (am.size() != bm.size()) return false;
  for (const auto& kv : am) {
    auto it = bm.find(kv.first);
    if (it == bm.end() || !structurally_equal(kv.second, it->second)) return false;
  }
  return true;
}

Map merge_maps(const Map& base, const Map& overlay) {
  Map merged = base;
  for (const auto& kv : overlay) {
    auto it = merged.find(kv.first);
    if (it != merged.end()) {
      const auto* left = std::get_if<Map>(&it->second.data);
      const auto* right = std::get_if<Map>(&kv.second.data);
      if (left && right) {
        Value nested;
        nested.data = merge_maps(*left, *right);
        it->second = std::move(nested);
        continue;
      }
    }
    merged[kv.first] = kv.second;
  }
  return merged;
}

}  // namespace helioforge::cfgscript
