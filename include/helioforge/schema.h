#pragma once

#include "helioforge/byte_tools.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace helioforge {

enum class FieldKind {
  kUnknown,
  kUnsigned,
  kVarint,
  kLengthPrefixedBytes,
  kText,
  kMarker,
  kPadding
};

struct FieldSpan {
  FieldKind kind = FieldKind::kUnknown;
  size_t offset = 0;
  size_t size = 0;
  uint64_t numeric_value = 0;
  std::string label;
};

struct SchemaGuess {
  std::string family;
  std::vector<FieldSpan> fields;
  std::vector<std::string> diagnostics;
  size_t consumed_bytes = 0;
};

SchemaGuess infer_binary_schema(const uint8_t* data, size_t size);
SchemaGuess infer_text_schema(const uint8_t* data, size_t size);
std::vector<FieldSpan> scan_length_prefixed_regions(const uint8_t* data, size_t size);
std::vector<FieldSpan> scan_varint_lanes(const uint8_t* data, size_t size);
std::vector<FieldSpan> coalesce_adjacent_fields(std::vector<FieldSpan> fields);
std::vector<FieldSpan> reject_overlapping_fields(const std::vector<FieldSpan>& fields);
std::map<FieldKind, size_t> summarize_field_kinds(const SchemaGuess& schema);
size_t estimate_schema_coverage(const SchemaGuess& schema, size_t input_size);
std::string field_kind_name(FieldKind kind);
std::string render_schema_guess(const SchemaGuess& schema);

}  // namespace helioforge
