#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace helioforge::binpack {

struct Error {
  std::string message;
  size_t offset = 0;
};

struct Record {
  uint16_t type = 0;
  uint32_t id = 0;
  std::vector<uint8_t> payload;
  std::vector<Record> children;
  std::map<std::string, std::string> attributes;
};

struct Section {
  uint16_t id = 0;
  uint16_t kind = 0;
  std::string name;
  uint32_t declared_checksum = 0;
  std::vector<uint8_t> payload;
  std::vector<Record> records;
  std::vector<uint16_t> references;
};

struct Container {
  uint16_t version = 0;
  std::map<std::string, std::string> metadata;
  std::vector<Section> sections;
  std::vector<uint16_t> index_order;
};

struct ParseResult {
  bool ok = false;
  Container value;
  std::vector<Error> errors;
};

ParseResult parse(const uint8_t* data, size_t size);
bool validate_references(const Container& container, std::vector<Error>* errors);
uint32_t checksum32(const uint8_t* data, size_t size);

}  // namespace helioforge::binpack
