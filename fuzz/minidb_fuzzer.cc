#include "minidb/minidb.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = helioforge::minidb::decode(data, size);
  if (!result.value.visible_records.empty()) {
    std::vector<helioforge::minidb::Record> replay;
    for (const auto& kv : result.value.visible_records) replay.push_back(kv.second);
    helioforge::minidb::replay(&result.value, replay);
  }
  return 0;
}
