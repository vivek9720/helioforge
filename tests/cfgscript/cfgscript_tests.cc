#include "cfgscript/cfgscript.h"

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
}
