#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace helioforge {

struct ByteWindow {
  size_t offset = 0;
  size_t size = 0;
  uint32_t rolling_hash = 0;
  double entropy = 0.0;
  bool mostly_text = false;
};

struct VarintRead {
  bool ok = false;
  uint64_t value = 0;
  size_t next = 0;
  size_t width = 0;
};

struct ByteProfile {
  size_t size = 0;
  size_t nul_bytes = 0;
  size_t ascii_bytes = 0;
  size_t high_bytes = 0;
  std::map<uint8_t, size_t> histogram;
  std::vector<ByteWindow> windows;
};

uint16_t read_le16(const uint8_t* data, size_t size, size_t offset, bool* ok);
uint32_t read_le32(const uint8_t* data, size_t size, size_t offset, bool* ok);
VarintRead read_varint(const uint8_t* data, size_t size, size_t offset);
uint32_t rolling_hash32(const uint8_t* data, size_t size);
double shannon_entropy(const uint8_t* data, size_t size);
bool looks_like_text(const uint8_t* data, size_t size);
ByteProfile profile_bytes(const uint8_t* data, size_t size, size_t window_size);
std::vector<size_t> find_marker(const uint8_t* data, size_t size, const std::string& marker);
std::string hex_preview(const uint8_t* data, size_t size, size_t limit);

}  // namespace helioforge
