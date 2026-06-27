#include "helioforge/byte_tools.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace helioforge {

uint16_t read_le16(const uint8_t* data, size_t size, size_t offset, bool* ok) {
  if (offset > size || size - offset < 2) {
    if (ok) *ok = false;
    return 0;
  }
  if (ok) *ok = true;
  return static_cast<uint16_t>(data[offset]) |
         static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t read_le32(const uint8_t* data, size_t size, size_t offset, bool* ok) {
  if (offset > size || size - offset < 4) {
    if (ok) *ok = false;
    return 0;
  }
  if (ok) *ok = true;
  return static_cast<uint32_t>(data[offset]) |
         (static_cast<uint32_t>(data[offset + 1]) << 8) |
         (static_cast<uint32_t>(data[offset + 2]) << 16) |
         (static_cast<uint32_t>(data[offset + 3]) << 24);
}

VarintRead read_varint(const uint8_t* data, size_t size, size_t offset) {
  VarintRead out;
  if (offset > size) return out;
  uint64_t value = 0;
  for (size_t i = 0; i < 10 && offset + i < size; ++i) {
    uint8_t b = data[offset + i];
    value |= static_cast<uint64_t>(b & 0x7fu) << (7u * i);
    if ((b & 0x80u) == 0) {
      out.ok = true;
      out.value = value;
      out.next = offset + i + 1;
      out.width = i + 1;
      return out;
    }
  }
  return out;
}

uint32_t rolling_hash32(const uint8_t* data, size_t size) {
  uint32_t h = 0x9e3779b9u;
  for (size_t i = 0; i < size; ++i) {
    h ^= static_cast<uint32_t>(data[i]) + 0x85ebca6bu + (h << 6) + (h >> 2);
    h = (h << 13) | (h >> 19);
  }
  return h;
}

double shannon_entropy(const uint8_t* data, size_t size) {
  if (size == 0) return 0.0;
  size_t counts[256] = {};
  for (size_t i = 0; i < size; ++i) ++counts[data[i]];
  double entropy = 0.0;
  for (size_t count : counts) {
    if (count == 0) continue;
    double p = static_cast<double>(count) / static_cast<double>(size);
    entropy -= p * std::log2(p);
  }
  return entropy;
}

bool looks_like_text(const uint8_t* data, size_t size) {
  if (size == 0) return false;
  size_t text = 0;
  size_t controls = 0;
  for (size_t i = 0; i < size; ++i) {
    uint8_t c = data[i];
    if ((c >= 0x20 && c <= 0x7e) || c == '\n' || c == '\r' || c == '\t') ++text;
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') ++controls;
  }
  return text * 4 >= size * 3 && controls * 10 < size;
}

ByteProfile profile_bytes(const uint8_t* data, size_t size, size_t window_size) {
  ByteProfile profile;
  profile.size = size;
  for (size_t i = 0; i < size; ++i) {
    ++profile.histogram[data[i]];
    if (data[i] == 0) ++profile.nul_bytes;
    else if (data[i] < 0x80) ++profile.ascii_bytes;
    else ++profile.high_bytes;
  }
  if (window_size == 0) window_size = 32;
  for (size_t off = 0; off < size;) {
    size_t take = std::min(window_size, size - off);
    ByteWindow window;
    window.offset = off;
    window.size = take;
    window.rolling_hash = rolling_hash32(data + off, take);
    window.entropy = shannon_entropy(data + off, take);
    window.mostly_text = looks_like_text(data + off, take);
    profile.windows.push_back(window);
    if (take == 0) break;
    off += take;
  }
  return profile;
}

std::vector<size_t> find_marker(const uint8_t* data, size_t size, const std::string& marker) {
  std::vector<size_t> hits;
  if (marker.empty() || marker.size() > size) return hits;
  const uint8_t* first = reinterpret_cast<const uint8_t*>(marker.data());
  for (size_t i = 0; i <= size - marker.size(); ++i) {
    if (std::equal(first, first + marker.size(), data + i)) hits.push_back(i);
  }
  return hits;
}

std::string hex_preview(const uint8_t* data, size_t size, size_t limit) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  size_t n = std::min(size, limit);
  for (size_t i = 0; i < n; ++i) {
    if (i) out << ' ';
    out << std::setw(2) << static_cast<unsigned>(data[i]);
  }
  if (n < size) out << " ...";
  return out.str();
}

}  // namespace helioforge
