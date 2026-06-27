#include "binpack/binpack.h"
#include "cfgscript/cfgscript.h"
#include "minidb/minidb.h"
#include "streamcodec/streamcodec.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: hf_inspect <binpack|minidb|cfgscript|streamcodec> <file>\n";
    return 2;
  }
  std::ifstream in(argv[2], std::ios::binary);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), {});
  std::string mode = argv[1];
  if (mode == "binpack") {
    auto r = helioforge::binpack::parse(data.data(), data.size());
    std::cout << "sections=" << r.value.sections.size() << " errors=" << r.errors.size() << "\n";
  } else if (mode == "minidb") {
    auto r = helioforge::minidb::decode(data.data(), data.size());
    std::cout << "pages=" << r.value.pages.size() << " records=" << r.value.visible_records.size()
              << " errors=" << r.errors.size() << "\n";
  } else if (mode == "cfgscript") {
    auto r = helioforge::cfgscript::parse(data.data(), data.size());
    std::cout << "globals=" << r.value.globals.size() << " includes=" << r.value.includes.size()
              << " errors=" << r.errors.size() << "\n";
  } else if (mode == "streamcodec") {
    std::vector<helioforge::streamcodec::Error> errors;
    auto m = helioforge::streamcodec::decode_stream(data.data(), data.size(), &errors);
    std::cout << "messages=" << m.size() << " errors=" << errors.size() << "\n";
  } else {
    return 2;
  }
  return 0;
}
