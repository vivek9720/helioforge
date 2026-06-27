#include "streamcodec/streamcodec.h"

#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  helioforge::streamcodec::Decoder decoder;
  std::vector<helioforge::streamcodec::Error> errors;
  size_t a = size / 3;
  size_t b = (size * 2) / 3;
  auto first = decoder.feed(data, a, &errors);
  auto second = decoder.feed(data + a, b - a, &errors);
  auto third = decoder.feed(data + b, size - b, &errors);
  volatile size_t total = first.size() + second.size() + third.size() + errors.size();
  (void)total;
  return 0;
}
