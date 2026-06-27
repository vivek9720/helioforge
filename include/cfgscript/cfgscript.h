#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace helioforge::cfgscript {

struct Value;
using Array = std::vector<Value>;
using Map = std::map<std::string, Value>;

struct Expr {
  std::string op;
  std::vector<std::string> atoms;
};

struct Value {
  std::variant<std::monostate, int64_t, bool, std::string, Array, Map, Expr> data;
};

struct IncludeRecord {
  std::string name;
  Map attributes;
};

struct Document {
  Map globals;
  std::vector<IncludeRecord> includes;
  std::vector<std::string> symbol_refs;
};

struct Error {
  std::string message;
  size_t offset = 0;
};

struct ParseResult {
  bool ok = false;
  Document value;
  std::vector<Error> errors;
};

ParseResult parse(const char* data, size_t size);
ParseResult parse(const uint8_t* data, size_t size);

}  // namespace helioforge::cfgscript
