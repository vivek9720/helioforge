#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace helioforge::streamcodec {

struct Message {
  uint32_t id = 0;
  uint8_t type = 0;
  std::vector<uint8_t> payload;
  std::map<std::string, std::string> metadata;
};

struct Error {
  std::string message;
  size_t offset = 0;
};

class Decoder {
 public:
  void reset();
  std::vector<Message> feed(const uint8_t* data, size_t size, std::vector<Error>* errors);

 private:
  std::vector<uint8_t> buffer_;
  std::map<uint32_t, Message> partial_;
  uint32_t rolling_ = 0;
};

std::vector<Message> decode_stream(const uint8_t* data, size_t size, std::vector<Error>* errors);

}  // namespace helioforge::streamcodec
