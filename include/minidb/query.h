#pragma once

#include "minidb/minidb.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace helioforge::minidb {

struct PageStats {
  uint32_t page_id = 0;
  uint32_t next_page = 0;
  size_t record_count = 0;
  size_t free_slot_count = 0;
  size_t encoded_field_count = 0;
};

struct StoreStats {
  size_t page_count = 0;
  size_t visible_record_count = 0;
  size_t string_table_bytes = 0;
  size_t integer_fields = 0;
  size_t string_fields = 0;
  size_t blob_fields = 0;
  size_t null_fields = 0;
  std::vector<PageStats> pages;
  std::vector<std::string> warnings;
};

struct Projection {
  uint32_t record_id = 0;
  std::map<size_t, std::string> printable_fields;
};

StoreStats analyze_store(const Database& db);
std::vector<uint32_t> page_chain(const Database& db, uint32_t start_page,
                                 std::vector<std::string>* warnings);
std::vector<Projection> project_records(const Database& db, const std::set<size_t>& columns);
std::vector<uint32_t> find_records_containing(const Database& db, const std::string& needle);
std::string field_to_string(const Field& field);
std::vector<uint8_t> export_compact_snapshot(const Database& db);

}  // namespace helioforge::minidb
