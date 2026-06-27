#include "binpack/binpack.h"
#include "binpack/manifest.h"
#include "cfgscript/cfgscript.h"
#include "cfgscript/evaluator.h"
#include "helioforge/crosscheck.h"
#include "minidb/minidb.h"
#include "minidb/query.h"
#include "streamcodec/streamcodec.h"
#include "streamcodec/session.h"

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
  if (mode == "summary") {
    auto report = helioforge::summarize_bytes(data.data(), data.size());
    std::cout << helioforge::render_crosscheck(report) << "\n";
  } else if (mode == "binpack") {
    auto r = helioforge::binpack::parse(data.data(), data.size());
    auto manifest = helioforge::binpack::build_manifest(r.value);
    std::cout << "sections=" << r.value.sections.size() << " records=" << manifest.records.size()
              << " errors=" << r.errors.size() << "\n";
  } else if (mode == "minidb") {
    auto r = helioforge::minidb::decode(data.data(), data.size());
    auto stats = helioforge::minidb::analyze_store(r.value);
    std::cout << "pages=" << r.value.pages.size() << " records=" << r.value.visible_records.size()
              << " fields=" << (stats.integer_fields + stats.string_fields + stats.blob_fields + stats.null_fields)
              << " errors=" << r.errors.size() << "\n";
  } else if (mode == "cfgscript") {
    auto r = helioforge::cfgscript::parse(data.data(), data.size());
    auto eval = helioforge::cfgscript::evaluate_document(r.value);
    std::cout << "globals=" << r.value.globals.size() << " includes=" << r.value.includes.size()
              << " eval_issues=" << eval.issues.size() << " errors=" << r.errors.size() << "\n";
  } else if (mode == "streamcodec") {
    std::vector<helioforge::streamcodec::Error> errors;
    auto m = helioforge::streamcodec::decode_stream(data.data(), data.size(), &errors);
    auto report = helioforge::streamcodec::analyze_stream_chunks(data.data(), data.size(), 11);
    std::cout << "messages=" << m.size() << " tracked=" << report.completed_messages
              << " errors=" << errors.size() << "\n";
  } else {
    return 2;
  }
  return 0;
}
