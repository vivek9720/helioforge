#include "streamcodec/streamcodec.h"

#include <cassert>
#include <vector>

static void u16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x & 255); v.push_back(x >> 8); }
static void u32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 255); }

int main() {
  std::vector<uint8_t> s = {'S','C',0,3};
  u32(s, 11); u32(s, 0); u16(s, 8); u16(s, 4); s.push_back(2);
  s.push_back(3); s.insert(s.end(), {'k','e','y'}); s.push_back(3); s.insert(s.end(), {'v','a','l'});
  s.insert(s.end(), {'d','a','t','a'});
  std::vector<helioforge::streamcodec::Error> errors;
  auto messages = helioforge::streamcodec::decode_stream(s.data(), s.size(), &errors);
  assert(messages.size() == 1);
  assert(messages[0].payload.size() == 4);
}
