#include "cfgscript/cfgscript.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = helioforge::cfgscript::parse(data, size);
  volatile size_t refs = result.value.symbol_refs.size();
  (void)refs;
  return 0;
}
