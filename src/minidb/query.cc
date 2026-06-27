#include "minidb/query.h"

#include <algorithm>
#include <sstream>

namespace helioforge::minidb {
namespace {

void append_var(std::vector<uint8_t>* out, uint64_t value) {
  while (value >= 128) {
    out->push_back(static_cast<uint8_t>((value & 0x7fu) | 0x80u));
    value >>= 7;
  }
  out->push_back(static_cast<uint8_t>(value));
}

void count_field(const Field& field, StoreStats* stats) {
  if (std::holds_alternative<std::monostate>(field)) {
    ++stats->null_fields;
  } else if (std::holds_alternative<int64_t>(field)) {
    ++stats->integer_fields;
  } else if (std::holds_alternative<std::string>(field)) {
    ++stats->string_fields;
  } else if (std::holds_alternative<std::vector<uint8_t>>(field)) {
    ++stats->blob_fields;
  }
}

bool field_contains(const Field& field, const std::string& needle) {
  if (const auto* s = std::get_if<std::string>(&field)) return s->find(needle) != std::string::npos;
  if (const auto* blob = std::get_if<std::vector<uint8_t>>(&field)) {
    if (needle.empty()) return true;
    return std::search(blob->begin(), blob->end(), needle.begin(), needle.end()) != blob->end();
  }
  return false;
}

}  // namespace

StoreStats analyze_store(const Database& db) {
  StoreStats stats;
  stats.page_count = db.pages.size();
  stats.visible_record_count = db.visible_records.size();
  for (const auto& kv : db.string_table) stats.string_table_bytes += kv.second.size();
  for (const auto& kv : db.pages) {
    const Page& page = kv.second;
    PageStats page_stats;
    page_stats.page_id = page.id;
    page_stats.next_page = page.next_page;
    page_stats.record_count = page.records.size();
    page_stats.free_slot_count = page.free_slots.size();
    std::set<uint16_t> free_seen;
    for (uint16_t slot : page.free_slots) {
      if (!free_seen.insert(slot).second) {
        stats.warnings.push_back("duplicate free slot on page " + std::to_string(page.id));
      }
    }
    for (const auto& record : page.records) {
      page_stats.encoded_field_count += record.fields.size();
      for (const auto& field : record.fields) count_field(field, &stats);
    }
    stats.pages.push_back(page_stats);
  }
  std::vector<std::string> chain_warnings;
  (void)page_chain(db, 1, &chain_warnings);
  stats.warnings.insert(stats.warnings.end(), chain_warnings.begin(), chain_warnings.end());
  return stats;
}

std::vector<uint32_t> page_chain(const Database& db, uint32_t start_page,
                                 std::vector<std::string>* warnings) {
  std::vector<uint32_t> chain;
  std::set<uint32_t> seen;
  uint32_t current = start_page;
  while (current != 0) {
    auto it = db.pages.find(current);
    if (it == db.pages.end()) {
      if (warnings) warnings->push_back("page chain references missing page " + std::to_string(current));
      break;
    }
    if (!seen.insert(current).second) {
      if (warnings) warnings->push_back("page chain cycle at page " + std::to_string(current));
      break;
    }
    chain.push_back(current);
    current = it->second.next_page;
  }
  return chain;
}

std::vector<Projection> project_records(const Database& db, const std::set<size_t>& columns) {
  std::vector<Projection> rows;
  for (const auto& kv : db.visible_records) {
    Projection projection;
    projection.record_id = kv.first;
    const Record& record = kv.second;
    if (columns.empty()) {
      for (size_t i = 0; i < record.fields.size(); ++i) {
        projection.printable_fields[i] = field_to_string(record.fields[i]);
      }
    } else {
      for (size_t column : columns) {
        if (column < record.fields.size()) {
          projection.printable_fields[column] = field_to_string(record.fields[column]);
        }
      }
    }
    rows.push_back(std::move(projection));
  }
  return rows;
}

std::vector<uint32_t> find_records_containing(const Database& db, const std::string& needle) {
  std::vector<uint32_t> ids;
  for (const auto& kv : db.visible_records) {
    for (const auto& field : kv.second.fields) {
      if (field_contains(field, needle)) {
        ids.push_back(kv.first);
        break;
      }
    }
  }
  return ids;
}

std::string field_to_string(const Field& field) {
  if (std::holds_alternative<std::monostate>(field)) return "null";
  if (const auto* n = std::get_if<int64_t>(&field)) return std::to_string(*n);
  if (const auto* s = std::get_if<std::string>(&field)) return *s;
  const auto* blob = std::get_if<std::vector<uint8_t>>(&field);
  std::ostringstream out;
  out << "bytes:";
  if (blob) {
    static const char hex[] = "0123456789abcdef";
    for (uint8_t b : *blob) {
      out << hex[b >> 4] << hex[b & 0xfu];
    }
  }
  return out.str();
}

std::vector<uint8_t> export_compact_snapshot(const Database& db) {
  std::vector<uint8_t> out;
  out.insert(out.end(), {'H', 'F', 'S', '1'});
  append_var(&out, db.visible_records.size());
  for (const auto& kv : db.visible_records) {
    append_var(&out, kv.first);
    append_var(&out, kv.second.fields.size());
    for (const auto& field : kv.second.fields) {
      std::string printable = field_to_string(field);
      append_var(&out, printable.size());
      out.insert(out.end(), printable.begin(), printable.end());
    }
  }
  return out;
}

}  // namespace helioforge::minidb
