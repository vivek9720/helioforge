#include "minidb/minidb.h"

#include <cassert>
#include <vector>

static void u16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x & 255); v.push_back(x >> 8); }
static void u32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 255); }
static void var(std::vector<uint8_t>& v, uint64_t x) { while (x >= 128) { v.push_back(static_cast<uint8_t>(x | 128)); x >>= 7; } v.push_back(static_cast<uint8_t>(x)); }

int main() {
  std::vector<uint8_t> rec;
  var(rec, 7); var(rec, 2); rec.push_back(1); var(rec, 84); rec.push_back(4); var(rec, 1);
  std::vector<uint8_t> db = {'M','D','B','1'};
  u16(db, 256); u16(db, 1); u16(db, 1); u16(db, 1);
  var(db, 1); var(db, 5); db.insert(db.end(), {'h','e','l','l','o'});
  u32(db, 1); u32(db, 0); u16(db, 1); u16(db, 0); var(db, rec.size()); db.insert(db.end(), rec.begin(), rec.end());
  var(db, 9); var(db, 1); db.push_back(2); var(db, 5); db.insert(db.end(), {'a','l','p','h','a'});
  auto decoded = helioforge::minidb::decode(db.data(), db.size());
  assert(decoded.ok);
  assert(decoded.value.visible_records.count(7) == 1);
  assert(decoded.value.visible_records.count(9) == 1);
}
