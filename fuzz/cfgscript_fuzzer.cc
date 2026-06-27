#include "cfgscript/cfgscript.h"
#include "cfgscript/evaluator.h"
#include "helioforge/crosscheck.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = helioforge::cfgscript::parse(data, size);
  auto eval = helioforge::cfgscript::evaluate_document(result.value);
  auto paths = helioforge::cfgscript::flatten_paths(eval.resolved);
  volatile size_t refs = result.value.symbol_refs.size() + paths.size() + eval.issues.size();
  (void)refs;
  auto cross = helioforge::summarize_bytes(data, size);
  volatile size_t cross_items = cross.reconstructed_items + cross.parser_error_count;
  (void)cross_items;
  return 0;
}
