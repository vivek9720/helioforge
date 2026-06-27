#include "helioforge/schema.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace helioforge {
namespace {

void add_field(SchemaGuess* guess, FieldKind kind, size_t offset, size_t size,
               uint64_t value, std::string label) {
  FieldSpan span;
  span.kind = kind;
  span.offset = offset;
  span.size = size;
  span.numeric_value = value;
  span.label = std::move(label);
  guess->fields.push_back(std::move(span));
  guess->consumed_bytes = std::max(guess->consumed_bytes, offset + size);
}

bool printable(uint8_t c) {
  return (c >= 0x20 && c <= 0x7e) || c == '\n' || c == '\r' || c == '\t';
}

std::vector<std::string> tokenize_text(const uint8_t* data, size_t size) {
  std::vector<std::string> tokens;
  size_t off = 0;
  while (off < size) {
    while (off < size && std::isspace(static_cast<unsigned char>(data[off]))) ++off;
    if (off >= size) break;
    if (std::isalnum(static_cast<unsigned char>(data[off])) || data[off] == '_' ||
        data[off] == '$') {
      size_t start = off++;
      while (off < size &&
             (std::isalnum(static_cast<unsigned char>(data[off])) || data[off] == '_' ||
              data[off] == '-' || data[off] == '.' || data[off] == '$')) {
        ++off;
      }
      tokens.emplace_back(reinterpret_cast<const char*>(data + start), off - start);
    } else {
      tokens.emplace_back(reinterpret_cast<const char*>(data + off), 1);
      ++off;
    }
  }
  return tokens;
}

size_t text_run_length(const uint8_t* data, size_t size, size_t offset) {
  size_t end = offset;
  while (end < size && printable(data[end]) && data[end] != '\0') ++end;
  return end - offset;
}

}  // namespace

SchemaGuess infer_binary_schema(const uint8_t* data, size_t size) {
  SchemaGuess guess;
  guess.family = "binary";
  if (size == 0) {
    guess.diagnostics.push_back("empty input");
    return guess;
  }

  if (size >= 4) {
    std::string marker(reinterpret_cast<const char*>(data), 4);
    bool marker_text = true;
    for (char c : marker) {
      if (!std::isprint(static_cast<unsigned char>(c))) marker_text = false;
    }
    if (marker_text) add_field(&guess, FieldKind::kMarker, 0, 4, 0, marker);
  }

  bool ok16 = false;
  bool ok32 = false;
  for (size_t off = 0; off + 4 <= std::min<size_t>(size, 64); off += 2) {
    uint16_t small = read_le16(data, size, off, &ok16);
    uint32_t wide = read_le32(data, size, off, &ok32);
    if (ok16 && small > 0 && small < size) {
      add_field(&guess, FieldKind::kUnsigned, off, 2, small, "small-le16");
    }
    if (ok32 && wide > 0 && wide < size && off + wide <= size) {
      add_field(&guess, FieldKind::kUnsigned, off, 4, wide, "bounded-le32");
    }
  }

  auto varints = scan_varint_lanes(data, size);
  guess.fields.insert(guess.fields.end(), varints.begin(), varints.end());
  auto regions = scan_length_prefixed_regions(data, size);
  guess.fields.insert(guess.fields.end(), regions.begin(), regions.end());

  for (size_t off = 0; off < size;) {
    size_t run = text_run_length(data, size, off);
    if (run >= 6) {
      add_field(&guess, FieldKind::kText, off, run, 0, "printable-run");
      off += run;
    } else {
      ++off;
    }
  }

  std::sort(guess.fields.begin(), guess.fields.end(),
            [](const FieldSpan& a, const FieldSpan& b) {
              if (a.offset != b.offset) return a.offset < b.offset;
              if (a.size != b.size) return a.size > b.size;
              return static_cast<int>(a.kind) < static_cast<int>(b.kind);
            });
  guess.fields = reject_overlapping_fields(coalesce_adjacent_fields(std::move(guess.fields)));
  if (guess.fields.size() > 256) {
    guess.fields.resize(256);
    guess.diagnostics.push_back("schema guess truncated to 256 fields");
  }
  return guess;
}

SchemaGuess infer_text_schema(const uint8_t* data, size_t size) {
  SchemaGuess guess;
  guess.family = "text";
  auto tokens = tokenize_text(data, size);
  size_t cursor = 0;
  for (const auto& token : tokens) {
    while (cursor < size) {
      std::string here(reinterpret_cast<const char*>(data + cursor),
                       std::min(token.size(), size - cursor));
      if (here == token) break;
      ++cursor;
    }
    FieldKind kind = FieldKind::kText;
    if (token == "{" || token == "}" || token == "[" || token == "]" ||
        token == "=" || token == ":" || token == "," || token == ";") {
      kind = FieldKind::kMarker;
    } else if (!token.empty() &&
               std::all_of(token.begin(), token.end(), [](char c) {
                 return std::isdigit(static_cast<unsigned char>(c)) || c == '-';
               })) {
      kind = FieldKind::kUnsigned;
    }
    add_field(&guess, kind, cursor, token.size(), 0, token);
    cursor += token.size();
    if (guess.fields.size() >= 256) {
      guess.diagnostics.push_back("text schema token limit reached");
      break;
    }
  }
  return guess;
}

std::vector<FieldSpan> scan_length_prefixed_regions(const uint8_t* data, size_t size) {
  std::vector<FieldSpan> spans;
  for (size_t off = 0; off + 1 < size && off < 512; ++off) {
    uint8_t len8 = data[off];
    if (len8 > 0 && len8 <= 64 && off + 1u + len8 <= size) {
      size_t text_bytes = 0;
      for (size_t i = 0; i < len8; ++i) {
        if (printable(data[off + 1 + i])) ++text_bytes;
      }
      FieldSpan span;
      span.kind = FieldKind::kLengthPrefixedBytes;
      span.offset = off;
      span.size = 1u + len8;
      span.numeric_value = len8;
      span.label = text_bytes * 2 >= len8 ? "len8-text" : "len8-bytes";
      spans.push_back(std::move(span));
    }
    bool ok = false;
    uint16_t len16 = read_le16(data, size, off, &ok);
    if (ok && len16 > 0 && len16 <= 4096 && off + 2u + len16 <= size) {
      FieldSpan span;
      span.kind = FieldKind::kLengthPrefixedBytes;
      span.offset = off;
      span.size = 2u + len16;
      span.numeric_value = len16;
      span.label = "len16-bytes";
      spans.push_back(std::move(span));
    }
  }
  if (spans.size() > 128) spans.resize(128);
  return spans;
}

std::vector<FieldSpan> scan_varint_lanes(const uint8_t* data, size_t size) {
  std::vector<FieldSpan> spans;
  for (size_t off = 0; off < size && off < 512; ++off) {
    VarintRead read = read_varint(data, size, off);
    if (!read.ok || read.width == 0 || read.width > 5) continue;
    if (read.value > size * 16u && read.value > 1024u) continue;
    FieldSpan span;
    span.kind = FieldKind::kVarint;
    span.offset = off;
    span.size = read.width;
    span.numeric_value = read.value;
    span.label = "varint";
    spans.push_back(std::move(span));
  }
  if (spans.size() > 128) spans.resize(128);
  return spans;
}

std::vector<FieldSpan> coalesce_adjacent_fields(std::vector<FieldSpan> fields) {
  if (fields.empty()) return fields;
  std::sort(fields.begin(), fields.end(), [](const FieldSpan& a, const FieldSpan& b) {
    if (a.offset != b.offset) return a.offset < b.offset;
    if (a.kind != b.kind) return static_cast<int>(a.kind) < static_cast<int>(b.kind);
    return a.size < b.size;
  });
  std::vector<FieldSpan> merged;
  for (auto field : fields) {
    if (!merged.empty()) {
      FieldSpan& back = merged.back();
      bool same_kind = back.kind == field.kind;
      bool touches = back.offset + back.size == field.offset;
      bool compatible_label = back.label == field.label || back.label.empty() || field.label.empty();
      if (same_kind && touches && compatible_label &&
          (field.kind == FieldKind::kText || field.kind == FieldKind::kPadding ||
           field.kind == FieldKind::kMarker)) {
        back.size += field.size;
        if (back.label.empty()) back.label = field.label;
        continue;
      }
    }
    merged.push_back(std::move(field));
  }
  return merged;
}

std::vector<FieldSpan> reject_overlapping_fields(const std::vector<FieldSpan>& fields) {
  std::vector<FieldSpan> ordered = fields;
  std::sort(ordered.begin(), ordered.end(), [](const FieldSpan& a, const FieldSpan& b) {
    if (a.offset != b.offset) return a.offset < b.offset;
    if (a.size != b.size) return a.size > b.size;
    return static_cast<int>(a.kind) < static_cast<int>(b.kind);
  });
  std::vector<FieldSpan> accepted;
  size_t covered_until = 0;
  for (const auto& field : ordered) {
    if (field.size == 0) continue;
    if (accepted.empty() || field.offset >= covered_until) {
      accepted.push_back(field);
      covered_until = field.offset + field.size;
      continue;
    }
    const FieldSpan& previous = accepted.back();
    bool previous_is_weak = previous.kind == FieldKind::kUnknown ||
                            previous.kind == FieldKind::kUnsigned ||
                            previous.kind == FieldKind::kVarint;
    bool current_is_rich = field.kind == FieldKind::kLengthPrefixedBytes ||
                           field.kind == FieldKind::kText ||
                           field.kind == FieldKind::kMarker;
    if (current_is_rich && previous_is_weak &&
        field.offset <= previous.offset && field.offset + field.size >= covered_until) {
      accepted.back() = field;
      covered_until = field.offset + field.size;
    }
  }
  return accepted;
}

std::map<FieldKind, size_t> summarize_field_kinds(const SchemaGuess& schema) {
  std::map<FieldKind, size_t> counts;
  for (const auto& field : schema.fields) ++counts[field.kind];
  return counts;
}

size_t estimate_schema_coverage(const SchemaGuess& schema, size_t input_size) {
  if (input_size == 0) return 0;
  std::vector<std::pair<size_t, size_t>> ranges;
  for (const auto& field : schema.fields) {
    if (field.offset >= input_size || field.size == 0) continue;
    size_t end = std::min(input_size, field.offset + field.size);
    ranges.push_back({field.offset, end});
  }
  if (ranges.empty()) return 0;
  std::sort(ranges.begin(), ranges.end());
  size_t covered = 0;
  size_t start = ranges.front().first;
  size_t end = ranges.front().second;
  for (size_t i = 1; i < ranges.size(); ++i) {
    if (ranges[i].first <= end) {
      end = std::max(end, ranges[i].second);
    } else {
      covered += end - start;
      start = ranges[i].first;
      end = ranges[i].second;
    }
  }
  covered += end - start;
  return covered;
}

std::string field_kind_name(FieldKind kind) {
  switch (kind) {
    case FieldKind::kUnknown: return "unknown";
    case FieldKind::kUnsigned: return "unsigned";
    case FieldKind::kVarint: return "varint";
    case FieldKind::kLengthPrefixedBytes: return "length-prefixed";
    case FieldKind::kText: return "text";
    case FieldKind::kMarker: return "marker";
    case FieldKind::kPadding: return "padding";
  }
  return "unknown";
}

std::string render_schema_guess(const SchemaGuess& schema) {
  std::ostringstream out;
  out << "family=" << schema.family << " fields=" << schema.fields.size()
      << " consumed=" << schema.consumed_bytes;
  auto counts = summarize_field_kinds(schema);
  for (const auto& kv : counts) out << " " << field_kind_name(kv.first) << "=" << kv.second;
  for (const auto& diagnostic : schema.diagnostics) out << "\nwarning: " << diagnostic;
  size_t shown = 0;
  for (const auto& field : schema.fields) {
    if (++shown > 32) {
      out << "\n...";
      break;
    }
    out << "\n@" << field.offset << "+" << field.size << " "
        << field_kind_name(field.kind) << " " << field.label;
    if (field.numeric_value != 0) out << "=" << field.numeric_value;
  }
  return out.str();
}

}  // namespace helioforge
