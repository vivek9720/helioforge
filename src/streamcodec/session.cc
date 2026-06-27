#include "streamcodec/session.h"

#include <algorithm>

namespace helioforge::streamcodec {
namespace {

void append32(std::vector<uint8_t>* out, uint32_t value) {
  out->push_back(static_cast<uint8_t>(value & 0xffu));
  out->push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
  out->push_back(static_cast<uint8_t>((value >> 16) & 0xffu));
  out->push_back(static_cast<uint8_t>((value >> 24) & 0xffu));
}

void update_stats(const Message& message, SessionReport* report) {
  auto& stats = report->messages[message.id];
  if (stats.fragments == 0) {
    stats.id = message.id;
    stats.type = message.type;
  }
  ++stats.fragments;
  stats.payload_bytes += message.payload.size();
  stats.last_metadata = message.metadata;
  stats.declared_compressed = stats.declared_compressed ||
                              message.metadata.find("compressed") != message.metadata.end();
  report->total_payload_bytes += message.payload.size();
  ++report->completed_messages;
}

}  // namespace

void SessionTracker::ingest(const uint8_t* data, size_t size) {
  std::vector<Error> local_errors;
  std::vector<Message> messages = decoder_.feed(data, size, &local_errors);
  for (const auto& err : local_errors) report_.errors.push_back(err);
  for (const auto& message : messages) {
    if (message.payload.empty()) {
      report_.warnings.push_back("message " + std::to_string(message.id) + " has empty payload");
    }
    if (message.metadata.find("rolling") == message.metadata.end()) {
      report_.warnings.push_back("message " + std::to_string(message.id) + " has no rolling state");
    }
    update_stats(message, &report_);
    completed_.push_back(message);
  }
}

std::vector<uint8_t> SessionTracker::replay_image() const {
  std::vector<uint8_t> image;
  image.insert(image.end(), {'H', 'F', 'R', '1'});
  append32(&image, static_cast<uint32_t>(completed_.size()));
  for (const auto& message : completed_) {
    append32(&image, message.id);
    append32(&image, message.type);
    append32(&image, static_cast<uint32_t>(message.payload.size()));
    image.insert(image.end(), message.payload.begin(), message.payload.end());
    append32(&image, static_cast<uint32_t>(message.metadata.size()));
    for (const auto& kv : message.metadata) {
      append32(&image, static_cast<uint32_t>(kv.first.size()));
      image.insert(image.end(), kv.first.begin(), kv.first.end());
      append32(&image, static_cast<uint32_t>(kv.second.size()));
      image.insert(image.end(), kv.second.begin(), kv.second.end());
    }
  }
  return image;
}

SessionReport analyze_stream_chunks(const uint8_t* data, size_t size, size_t chunk_hint) {
  SessionTracker tracker;
  if (chunk_hint == 0) chunk_hint = 1;
  size_t offset = 0;
  while (offset < size) {
    size_t take = std::min(chunk_hint + (offset % 5), size - offset);
    tracker.ingest(data + offset, take);
    offset += take;
  }
  return tracker.report();
}

std::map<std::string, size_t> metadata_histogram(const std::vector<Message>& messages) {
  std::map<std::string, size_t> histogram;
  for (const auto& message : messages) {
    for (const auto& kv : message.metadata) ++histogram[kv.first];
  }
  return histogram;
}

std::vector<uint32_t> message_order(const std::vector<Message>& messages) {
  std::vector<uint32_t> order;
  for (const auto& message : messages) order.push_back(message.id);
  order.erase(std::unique(order.begin(), order.end()), order.end());
  return order;
}

}  // namespace helioforge::streamcodec
