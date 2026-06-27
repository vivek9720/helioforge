#include "helioforge/crosscheck.h"
#include "minidb/minidb.h"
#include "minidb/query.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto result = helioforge::minidb::decode(data, size);
  if (!result.value.visible_records.empty()) {
    std::vector<helioforge::minidb::Record> replay;
    for (const auto& kv : result.value.visible_records) replay.push_back(kv.second);
    helioforge::minidb::replay(&result.value, replay);
    auto stats = helioforge::minidb::analyze_store(result.value);
    auto snapshot = helioforge::minidb::export_compact_snapshot(result.value);
    volatile size_t observed = stats.visible_record_count + snapshot.size();
    (void)observed;
  }
  auto cross = helioforge::summarize_bytes(data, size);
  volatile size_t cross_items = cross.reconstructed_items + cross.parser_error_count;
  (void)cross_items;
  return 0;
}
