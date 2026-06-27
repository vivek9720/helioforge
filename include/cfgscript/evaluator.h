#pragma once

#include "cfgscript/cfgscript.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace helioforge::cfgscript {

struct EvalIssue {
  std::string path;
  std::string message;
};

struct Evaluation {
  Map resolved;
  std::vector<EvalIssue> issues;
  std::set<std::string> referenced_symbols;
  std::set<std::string> include_names;
};

class Environment {
 public:
  void set(std::string name, Value value);
  bool has(const std::string& name) const;
  const Value* get(const std::string& name) const;
  Evaluation evaluate(const Document& doc) const;

 private:
  std::map<std::string, Value> symbols_;
};

Evaluation evaluate_document(const Document& doc);
std::vector<std::string> flatten_paths(const Map& map);
std::string value_to_string(const Value& value);
bool structurally_equal(const Value& a, const Value& b);
Map merge_maps(const Map& base, const Map& overlay);

}  // namespace helioforge::cfgscript
