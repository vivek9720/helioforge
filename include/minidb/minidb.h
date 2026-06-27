#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace helioforge::minidb {

using Field = std::variant<std::monostate, int64_t, std::string, std::vector<uint8_t>>;

struct Record {
  uint32_t id = 0;
  std::vector<Field> fields;
};

struct Page {
  uint32_t id = 0;
  uint32_t next_page = 0;
  std::vector<Record> records;
  std::vector<uint16_t> free_slots;
};

struct Database {
  uint16_t page_size = 0;
  std::map<uint32_t, Page> pages;
  std::map<uint32_t, std::string> string_table;
  std::map<uint32_t, Record> visible_records;
};

struct Error {
  std::string message;
  size_t offset = 0;
};

struct DecodeResult {
  bool ok = false;
  Database value;
  std::vector<Error> errors;
};

DecodeResult decode(const uint8_t* data, size_t size);
void replay(Database* db, const std::vector<Record>& journal);

}  // namespace helioforge::minidb
