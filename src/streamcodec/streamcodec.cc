#include "streamcodec/streamcodec.h"

#include <cstddef>

namespace helioforge::streamcodec {
namespace {

uint16_t rd16(const std::vector<uint8_t>& b, size_t p) {
  return static_cast<uint16_t>(b[p]) | static_cast<uint16_t>(b[p + 1] << 8);
}

uint32_t rd32(const std::vector<uint8_t>& b, size_t p) {
  return static_cast<uint32_t>(b[p]) | (static_cast<uint32_t>(b[p + 1]) << 8) |
         (static_cast<uint32_t>(b[p + 2]) << 16) | (static_cast<uint32_t>(b[p + 3]) << 24);
}

void meta_parse(const uint8_t* p, size_t n, std::map<std::string, std::string>* out) {
  size_t off = 0;
  while (off < n) {
    uint8_t k = p[off++];
    if (off + k > n) break;
    std::string key(reinterpret_cast<const char*>(p + off), k);
    off += k;
    if (off >= n) break;
    uint8_t v = p[off++];
    if (off + v > n) break;
    std::string val(reinterpret_cast<const char*>(p + off), v);
    off += v;
    (*out)[key] = val;
  }
}

}  // namespace

void Decoder::reset() {
  buffer_.clear();
  partial_.clear();
  rolling_ = 0;
}

std::vector<Message> Decoder::feed(const uint8_t* data, size_t size, std::vector<Error>* errors) {
  buffer_.insert(buffer_.end(), data, data + size);
  std::vector<Message> out;
  size_t pos = 0;
  while (buffer_.size() - pos >= 17) {
    if (!(buffer_[pos] == 'S' && buffer_[pos + 1] == 'C')) {
      if (errors) errors->push_back({"frame marker not found", pos});
      ++pos;
      continue;
    }
    uint8_t flags = buffer_[pos + 2];
    uint8_t type = buffer_[pos + 3];
    uint32_t id = rd32(buffer_, pos + 4);
    uint32_t seq = rd32(buffer_, pos + 8);
    uint16_t meta_len = rd16(buffer_, pos + 12);
    uint16_t payload_len = rd16(buffer_, pos + 14);
    uint8_t state = buffer_[pos + 16];
    size_t frame_len = 17u + meta_len + payload_len;
    if (buffer_.size() - pos < frame_len) break;
    const uint8_t* meta = buffer_.data() + pos + 17;
    const uint8_t* payload = meta + meta_len;
    rolling_ = (rolling_ * 131u) ^ id ^ seq ^ state ^ flags;

    Message frame;
    frame.id = id;
    frame.type = type;
    meta_parse(meta, meta_len, &frame.metadata);
    frame.payload.assign(payload, payload + payload_len);
    frame.metadata["rolling"] = std::to_string(rolling_);
    if (flags & 0x01u) frame.metadata["compressed"] = "declared";

    if (flags & 0x02u) {
      auto& acc = partial_[id];
      if (acc.payload.empty()) {
        acc.id = id;
        acc.type = type;
        acc.metadata = frame.metadata;
      }
      acc.payload.insert(acc.payload.end(), frame.payload.begin(), frame.payload.end());
      acc.metadata["last_seq"] = std::to_string(seq);
      if (state == 2) {
        out.push_back(acc);
        partial_.erase(id);
      }
    } else if (flags & 0x04u) {
      auto it = partial_.find(id);
      if (it == partial_.end()) {
        if (errors) errors->push_back({"continuation without start", pos});
      } else {
        it->second.payload.insert(it->second.payload.end(), frame.payload.begin(), frame.payload.end());
        if (state == 2) {
          out.push_back(it->second);
          partial_.erase(it);
        }
      }
    } else {
      out.push_back(std::move(frame));
    }
    pos += frame_len;
  }
  if (pos > 0) buffer_.erase(buffer_.begin(), buffer_.begin() + static_cast<std::ptrdiff_t>(pos));
  return out;
}

std::vector<Message> decode_stream(const uint8_t* data, size_t size, std::vector<Error>* errors) {
  Decoder d;
  size_t split = size / 2;
  std::vector<Message> result = d.feed(data, split, errors);
  std::vector<Message> tail = d.feed(data + split, size - split, errors);
  result.insert(result.end(), tail.begin(), tail.end());
  return result;
}

}  // namespace helioforge::streamcodec
