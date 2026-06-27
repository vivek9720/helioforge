#include "minidb/minidb.h"

#include <set>

namespace helioforge::minidb {
namespace {

class Reader {
 public:
  Reader(const uint8_t* d, size_t s) : data_(d), size_(s) {}
  size_t offset() const { return off_; }
  size_t remaining() const { return off_ <= size_ ? size_ - off_ : 0; }
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
  bool varint(uint64_t* out) {
    uint64_t value = 0;
    for (int shift = 0; shift <= 63; shift += 7) {
      uint8_t b = 0;
      if (!u8(&b)) return false;
      value |= static_cast<uint64_t>(b & 0x7f) << shift;
      if ((b & 0x80) == 0) {
        *out = value;
        return true;
      }
    }
    return false;
  }
  bool bytes(size_t n, std::vector<uint8_t>* out) {
    if (remaining() < n) return false;
    out->assign(data_ + off_, data_ + off_ + n);
    off_ += n;
    return true;
  }

 private:
  const uint8_t* data_;
  size_t size_;
  size_t off_ = 0;
};

void err(DecodeResult* r, size_t off, const char* msg) {
  r->errors.push_back({msg, off});
}

bool parse_field(Reader* rd, const Database& db, Field* field, DecodeResult* res) {
  uint8_t type = 0;
  if (!rd->u8(&type)) return false;
  uint64_t n = 0;
  switch (type) {
    case 0:
      *field = std::monostate{};
      return true;
    case 1:
      if (!rd->varint(&n)) return false;
      *field = static_cast<int64_t>((n >> 1) ^ (~(n & 1) + 1));
      return true;
    case 2: {
      if (!rd->varint(&n) || n > rd->remaining()) return false;
      std::vector<uint8_t> bytes;
      if (!rd->bytes(static_cast<size_t>(n), &bytes)) return false;
      *field = std::string(bytes.begin(), bytes.end());
      return true;
    }
    case 3: {
      if (!rd->varint(&n) || n > rd->remaining()) return false;
      std::vector<uint8_t> bytes;
      if (!rd->bytes(static_cast<size_t>(n), &bytes)) return false;
      *field = std::move(bytes);
      return true;
    }
    case 4: {
      if (!rd->varint(&n)) return false;
      auto it = db.string_table.find(static_cast<uint32_t>(n));
      if (it == db.string_table.end()) {
        err(res, rd->offset(), "missing string table reference");
        *field = std::string{};
      } else {
        *field = it->second;
      }
      return true;
    }
    default:
      err(res, rd->offset(), "unknown field type");
      return false;
  }
}

bool parse_record(Reader* rd, const Database& db, Record* rec, DecodeResult* res) {
  uint64_t id = 0;
  uint64_t count = 0;
  if (!rd->varint(&id) || !rd->varint(&count)) return false;
  rec->id = static_cast<uint32_t>(id);
  if (count > 128) {
    err(res, rd->offset(), "record has too many fields");
    return false;
  }
  for (uint64_t i = 0; i < count; ++i) {
    Field f;
    if (!parse_field(rd, db, &f, res)) return false;
    rec->fields.push_back(std::move(f));
  }
  return true;
}

bool apply_page_chain(Database* db, DecodeResult* res) {
  std::set<uint32_t> visited;
  uint32_t id = 1;
  while (id != 0) {
    auto it = db->pages.find(id);
    if (it == db->pages.end()) {
      err(res, id, "page chain references missing page");
      return false;
    }
    if (!visited.insert(id).second) {
      err(res, id, "cycle in page chain");
      return false;
    }
    for (const auto& rec : it->second.records) db->visible_records[rec.id] = rec;
    id = it->second.next_page;
  }
  return true;
}

}  // namespace

void replay(Database* db, const std::vector<Record>& journal) {
  for (const auto& rec : journal) {
    if (rec.fields.empty()) {
      db->visible_records.erase(rec.id);
    } else {
      db->visible_records[rec.id] = rec;
    }
  }
}

DecodeResult decode(const uint8_t* data, size_t size) {
  DecodeResult res;
  Reader rd(data, size);
  uint8_t magic[4] = {};
  for (auto& b : magic) {
    if (!rd.u8(&b)) return res;
  }
  if (!(magic[0] == 'M' && magic[1] == 'D' && magic[2] == 'B' && magic[3] == '1')) {
    err(&res, 0, "bad minidb magic");
  }
  uint16_t pages = 0;
  uint16_t strings = 0;
  uint16_t journal_count = 0;
  if (!rd.u16(&res.value.page_size) || !rd.u16(&pages) || !rd.u16(&strings) ||
      !rd.u16(&journal_count)) {
    return res;
  }
  for (uint16_t i = 0; i < strings; ++i) {
    uint64_t id = 0;
    uint64_t len = 0;
    if (!rd.varint(&id) || !rd.varint(&len) || len > rd.remaining()) return res;
    std::vector<uint8_t> bytes;
    if (!rd.bytes(static_cast<size_t>(len), &bytes)) return res;
    res.value.string_table[static_cast<uint32_t>(id)] =
        std::string(bytes.begin(), bytes.end());
  }
  for (uint16_t i = 0; i < pages; ++i) {
    Page page;
    uint16_t slots = 0;
    uint16_t frees = 0;
    if (!rd.u32(&page.id) || !rd.u32(&page.next_page) || !rd.u16(&slots) ||
        !rd.u16(&frees)) {
      return res;
    }
    for (uint16_t j = 0; j < frees; ++j) {
      uint16_t slot = 0;
      if (!rd.u16(&slot)) return res;
      page.free_slots.push_back(slot);
    }
    for (uint16_t j = 0; j < slots; ++j) {
      uint64_t len = 0;
      if (!rd.varint(&len) || len > rd.remaining()) return res;
      std::vector<uint8_t> slot;
      if (!rd.bytes(static_cast<size_t>(len), &slot)) return res;
      Reader sr(slot.data(), slot.size());
      Record rec;
      if (parse_record(&sr, res.value, &rec, &res)) page.records.push_back(std::move(rec));
    }
    res.value.pages[page.id] = std::move(page);
  }
  std::vector<Record> journal;
  for (uint16_t i = 0; i < journal_count; ++i) {
    Record rec;
    if (!parse_record(&rd, res.value, &rec, &res)) return res;
    journal.push_back(std::move(rec));
  }
  apply_page_chain(&res.value, &res);
  replay(&res.value, journal);
  res.ok = res.errors.empty();
  return res;
}

}  // namespace helioforge::minidb
