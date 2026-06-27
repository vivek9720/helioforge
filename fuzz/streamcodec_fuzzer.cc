#include "helioforge/crosscheck.h"
#include "streamcodec/streamcodec.h"
#include "streamcodec/session.h"

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
  auto report = helioforge::streamcodec::analyze_stream_chunks(data, size, 7);
  volatile size_t total = first.size() + second.size() + third.size() + errors.size() +
                          report.completed_messages + report.errors.size();
  (void)total;
  auto cross = helioforge::summarize_bytes(data, size);
  volatile size_t cross_items = cross.reconstructed_items + cross.parser_error_count;
  (void)cross_items;
  return 0;
}
