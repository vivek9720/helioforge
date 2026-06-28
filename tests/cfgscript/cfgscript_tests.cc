#include "cfgscript/cfgscript.h"
#include "cfgscript/evaluator.h"

#include <cassert>
#include <cstring>

int main() {
  const char* text =
      "include \"defaults\" { mode: \"base\" }\n"
      "name = \"node\\n01\";\n"
      "limits = { low: 1, high: 7, tags: [\"a\", $ref(other)] };\n"
      "enabled = true;\n";
  auto parsed = helioforge::cfgscript::parse(text, std::strlen(text));
  assert(parsed.ok);
  assert(parsed.value.includes.size() == 1);
  assert(parsed.value.globals.count("limits") == 1);
  auto eval = helioforge::cfgscript::evaluate_document(parsed.value);
  assert(eval.resolved.count("limits") == 1);
  assert(!helioforge::cfgscript::flatten_paths(eval.resolved).empty());

  const char* self_ref = "a = $a;\n";
  auto self = helioforge::cfgscript::parse(self_ref, std::strlen(self_ref));
  assert(self.ok);
  auto self_eval = helioforge::cfgscript::evaluate_document(self.value);
  assert(self_eval.resolved.count("a") == 1);
  assert(!self_eval.issues.empty());

  const char* cycle_ref = "a = $b; b = $a;\n";
  auto cycle = helioforge::cfgscript::parse(cycle_ref, std::strlen(cycle_ref));
  assert(cycle.ok);
  auto cycle_eval = helioforge::cfgscript::evaluate_document(cycle.value);
  assert(cycle_eval.resolved.count("a") == 1);
  assert(cycle_eval.resolved.count("b") == 1);
  assert(!cycle_eval.issues.empty());

  const char* concat_ref = "a = $concat(a);\n";
  auto concat = helioforge::cfgscript::parse(concat_ref, std::strlen(concat_ref));
  assert(concat.ok);
  auto concat_eval = helioforge::cfgscript::evaluate_document(concat.value);
  assert(concat_eval.resolved.count("a") == 1);
}
