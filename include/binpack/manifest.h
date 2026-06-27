#pragma once

#include "binpack/binpack.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace helioforge::binpack {

struct RecordSummary {
  uint16_t type = 0;
  uint32_t id = 0;
  size_t payload_size = 0;
  size_t descendant_count = 0;
  std::map<std::string, std::string> attributes;
};

struct SectionSummary {
  uint16_t id = 0;
  uint16_t kind = 0;
  std::string name;
  size_t payload_size = 0;
  size_t record_count = 0;
  size_t nested_record_count = 0;
  uint32_t computed_checksum = 0;
  std::vector<uint16_t> outgoing_refs;
  std::vector<uint16_t> incoming_refs;
};

struct Manifest {
  uint16_t version = 0;
  std::map<std::string, std::string> metadata;
  std::vector<SectionSummary> sections;
  std::vector<RecordSummary> records;
  std::vector<std::string> warnings;
};

struct ReferenceGraph {
  std::map<uint16_t, std::set<uint16_t>> outgoing;
  std::map<uint16_t, std::set<uint16_t>> incoming;
  std::vector<std::vector<uint16_t>> chains;
  std::vector<uint16_t> missing;
};

Manifest build_manifest(const Container& container);
ReferenceGraph build_reference_graph(const Container& container);
std::vector<const Record*> find_records_by_type(const Container& container, uint16_t type);
std::vector<uint8_t> canonical_payload_digest(const Container& container);
std::string describe_manifest(const Manifest& manifest);

}  // namespace helioforge::binpack
