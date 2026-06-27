#include "binpack/binpack.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = helioforge::binpack::parse(data, size);
  if (!result.value.sections.empty()) {
    (void)helioforge::binpack::validate_references(result.value, nullptr);
  }
  return 0;
}
