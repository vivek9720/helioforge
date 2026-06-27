#include "binpack/binpack.h"
#include "binpack/manifest.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using helioforge::binpack::checksum32;

static void u16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x & 255); v.push_back(x >> 8); }
static void u32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 255); }
static void s8(std::vector<uint8_t>& v, const char* s) { size_t n = std::char_traits<char>::length(s); v.push_back(static_cast<uint8_t>(n)); v.insert(v.end(), s, s + n); }

int main() {
  std::vector<uint8_t> payload;
  u16(payload, 1);
  u16(payload, 7); u32(payload, 42); u32(payload, 3); u16(payload, 1); u16(payload, 0);
  payload.insert(payload.end(), {'a','b','c'});
  s8(payload, "role"); s8(payload, "root");
  std::vector<uint8_t> f = {'B','P','K','1'};
  u16(f, 1); u16(f, 1); u16(f, 1); u16(f, 1); s8(f, "author"); s8(f, "local");
  uint32_t off = 4 + 2 + 2 + 2 + 2 + 1 + 6 + 1 + 5 + 2 + 2 + 4 + 4 + 4 + 1 + 4 + 2 + 2;
  u16(f, 10); u16(f, 1); u32(f, off); u32(f, static_cast<uint32_t>(payload.size()));
  u32(f, checksum32(payload.data(), payload.size())); s8(f, "data"); u16(f, 1); u16(f, 10);
  f.insert(f.end(), payload.begin(), payload.end());
  auto parsed = helioforge::binpack::parse(f.data(), f.size());
  assert(parsed.ok);
  assert(parsed.value.sections.size() == 1);
  assert(parsed.value.sections[0].records.size() == 1);
  auto manifest = helioforge::binpack::build_manifest(parsed.value);
  assert(manifest.records.size() == 1);
  assert(helioforge::binpack::find_records_by_type(parsed.value, 7).size() == 1);
}
