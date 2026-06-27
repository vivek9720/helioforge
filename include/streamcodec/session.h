#pragma once

#include "streamcodec/streamcodec.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace helioforge::streamcodec {

struct MessageStats {
  uint32_t id = 0;
  uint8_t type = 0;
  size_t fragments = 0;
  size_t payload_bytes = 0;
  bool declared_compressed = false;
  std::map<std::string, std::string> last_metadata;
};

struct SessionReport {
  std::map<uint32_t, MessageStats> messages;
  std::vector<Error> errors;
  std::vector<std::string> warnings;
  size_t total_payload_bytes = 0;
  size_t completed_messages = 0;
};

class SessionTracker {
 public:
  void ingest(const uint8_t* data, size_t size);
  const SessionReport& report() const { return report_; }
  std::vector<uint8_t> replay_image() const;

 private:
  Decoder decoder_;
  SessionReport report_;
  std::vector<Message> completed_;
};

SessionReport analyze_stream_chunks(const uint8_t* data, size_t size, size_t chunk_hint);
std::map<std::string, size_t> metadata_histogram(const std::vector<Message>& messages);
std::vector<uint32_t> message_order(const std::vector<Message>& messages);

}  // namespace helioforge::streamcodec
