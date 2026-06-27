#include "binpack/binpack.h"

#include <algorithm>
#include <set>

namespace helioforge::binpack {
namespace {

class Reader {
 public:
  Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}
  size_t offset() const { return off_; }
  size_t remaining() const { return off_ <= size_ ? size_ - off_ : 0; }
  bool skip(size_t n) {
    if (remaining() < n) return false;
    off_ += n;
    return true;
  }
  bool u8(uint8_t* out) {
    if (remaining() < 1) return false;
    *out = data_[off_++];
    return true;
  }
  bool u16(uint16_t* out) {
    if (remaining() < 2) return false;
    *out = static_cast<uint16_t>(data_[off_]) |
           static_cast<uint16_t>(data_[off_ + 1] << 8);
    off_ += 2;
    return true;
  }
  bool u32(uint32_t* out) {
    if (remaining() < 4) return false;
    *out = static_cast<uint32_t>(data_[off_]) |
           (static_cast<uint32_t>(data_[off_ + 1]) << 8) |
           (static_cast<uint32_t>(data_[off_ + 2]) << 16) |
           (static_cast<uint32_t>(data_[off_ + 3]) << 24);
    off_ += 4;
    return true;
  }
  bool bytes(size_t n, std::vector<uint8_t>* out) {
    if (remaining() < n) return false;
    out->assign(data_ + off_, data_ + off_ + n);
    off_ += n;
    return true;
  }
  bool string8(std::string* out) {
    uint8_t n = 0;
    if (!u8(&n) || remaining() < n) return false;
    out->assign(reinterpret_cast<const char*>(data_ + off_), n);
    off_ += n;
    return true;
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t off_ = 0;
};

void add_error(ParseResult* result, size_t off, const std::string& msg) {
  result->errors.push_back({msg, off});
}

bool parse_metadata(Reader* r, std::map<std::string, std::string>* meta,
                    ParseResult* result) {
  uint16_t count = 0;
  if (!r->u16(&count)) return false;
  for (uint16_t i = 0; i < count; ++i) {
    std::string key;
    std::string value;
    if (!r->string8(&key) || !r->string8(&value)) return false;
    if (key.empty()) add_error(result, r->offset(), "empty metadata key");
    (*meta)[key] = value;
  }
  return true;
}

bool parse_record(Reader* r, int depth, Record* rec, ParseResult* result) {
  if (depth > 12) {
    add_error(result, r->offset(), "record nesting limit reached");
    return false;
  }
  uint16_t attr_count = 0;
  uint16_t child_count = 0;
  uint32_t payload_len = 0;
  if (!r->u16(&rec->type) || !r->u32(&rec->id) || !r->u32(&payload_len) ||
      !r->u16(&attr_count) || !r->u16(&child_count)) {
    return false;
  }
  if (payload_len > r->remaining()) return false;
  if (!r->bytes(payload_len, &rec->payload)) return false;
  for (uint16_t i = 0; i < attr_count; ++i) {
    std::string key;
    std::string value;
    if (!r->string8(&key) || !r->string8(&value)) return false;
    rec->attributes[key] = value;
  }
  for (uint16_t i = 0; i < child_count; ++i) {
    Record child;
    if (!parse_record(r, depth + 1, &child, result)) return false;
    rec->children.push_back(std::move(child));
  }
  return true;
}

void parse_section_payload(Section* section, ParseResult* result) {
  Reader r(section->payload.data(), section->payload.size());
  if (section->kind == 1) {
    uint16_t records = 0;
    if (!r.u16(&records)) return;
    for (uint16_t i = 0; i < records && r.remaining() > 0; ++i) {
      Record rec;
      if (!parse_record(&r, 0, &rec, result)) {
        add_error(result, r.offset(), "truncated nested record");
        break;
      }
      section->records.push_back(std::move(rec));
    }
  } else if (section->kind == 2) {
    uint16_t refs = 0;
    if (!r.u16(&refs)) return;
    for (uint16_t i = 0; i < refs; ++i) {
      uint16_t ref = 0;
      if (!r.u16(&ref)) break;
      section->references.push_back(ref);
    }
  }
}

}  // namespace

uint32_t checksum32(const uint8_t* data, size_t size) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < size; ++i) {
    h ^= data[i];
    h *= 16777619u;
    h = (h << 5) | (h >> 27);
  }
  return h;
}

bool validate_references(const Container& c, std::vector<Error>* errors) {
  std::set<uint16_t> ids;
  bool ok = true;
  for (const auto& s : c.sections) ids.insert(s.id);
  for (const auto& s : c.sections) {
    for (uint16_t ref : s.references) {
      if (!ids.count(ref)) {
        ok = false;
        if (errors) errors->push_back({"section reference is missing", s.id});
      }
    }
  }
  for (uint16_t id : c.index_order) {
    if (!ids.count(id)) {
      ok = false;
      if (errors) errors->push_back({"index references missing section", id});
    }
  }
  return ok;
}

ParseResult parse(const uint8_t* data, size_t size) {
  ParseResult result;
  Reader r(data, size);
  uint8_t magic[4] = {};
  for (uint8_t& b : magic) {
    if (!r.u8(&b)) {
      add_error(&result, r.offset(), "truncated magic");
      return result;
    }
  }
  if (!(magic[0] == 'B' && magic[1] == 'P' && magic[2] == 'K' && magic[3] == '1')) {
    add_error(&result, 0, "bad binpack magic");
  }
  if (!r.u16(&result.value.version)) return result;
  uint16_t flags = 0;
  uint16_t sections = 0;
  if (!r.u16(&flags) || !r.u16(&sections)) return result;
  if (!parse_metadata(&r, &result.value.metadata, &result)) return result;

  struct Entry {
    uint16_t id;
    uint16_t kind;
    uint32_t off;
    uint32_t len;
    uint32_t checksum;
    std::string name;
  };
  std::vector<Entry> table;
  for (uint16_t i = 0; i < sections; ++i) {
    Entry e{};
    if (!r.u16(&e.id) || !r.u16(&e.kind) || !r.u32(&e.off) || !r.u32(&e.len) ||
        !r.u32(&e.checksum) || !r.string8(&e.name)) {
      add_error(&result, r.offset(), "truncated section table");
      return result;
    }
    table.push_back(e);
  }
  if (flags & 1u) {
    uint16_t count = 0;
    if (!r.u16(&count)) return result;
    for (uint16_t i = 0; i < count; ++i) {
      uint16_t id = 0;
      if (!r.u16(&id)) return result;
      result.value.index_order.push_back(id);
    }
  }

  for (const auto& e : table) {
    if (e.off > size || e.len > size - e.off) {
      add_error(&result, e.off, "section outside container");
      continue;
    }
    Section s;
    s.id = e.id;
    s.kind = e.kind;
    s.name = e.name;
    s.declared_checksum = e.checksum;
    s.payload.assign(data + e.off, data + e.off + e.len);
    uint32_t actual = checksum32(s.payload.data(), s.payload.size());
    if (s.declared_checksum != 0 && actual != s.declared_checksum) {
      add_error(&result, e.off, "section checksum mismatch");
    }
    parse_section_payload(&s, &result);
    result.value.sections.push_back(std::move(s));
  }
  validate_references(result.value, &result.errors);
  result.ok = result.errors.empty();
  return result;
}

}  // namespace helioforge::binpack
