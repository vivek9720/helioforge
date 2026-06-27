#include "binpack/binpack.h"
#include "binpack/manifest.h"
#include "helioforge/crosscheck.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = helioforge::binpack::parse(data, size);
  if (!result.value.sections.empty()) {
    (void)helioforge::binpack::validate_references(result.value, nullptr);
    auto manifest = helioforge::binpack::build_manifest(result.value);
    auto digest = helioforge::binpack::canonical_payload_digest(result.value);
    volatile size_t observed = manifest.records.size() + digest.size();
    (void)observed;
  }
  auto cross = helioforge::summarize_bytes(data, size);
  volatile size_t cross_items = cross.reconstructed_items + cross.parser_error_count;
  (void)cross_items;
  return 0;
}
