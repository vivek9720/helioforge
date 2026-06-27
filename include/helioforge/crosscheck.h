#pragma once

#include "helioforge/byte_tools.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace helioforge {

struct CandidateSignal {
  std::string subsystem;
  size_t confidence = 0;
  std::vector<std::string> notes;
};

struct CrossCheckReport {
  ByteProfile profile;
  std::vector<CandidateSignal> signals;
  std::vector<std::string> parser_notes;
  size_t parser_error_count = 0;
  size_t reconstructed_items = 0;
  size_t inferred_fields = 0;
  size_t rule_hits = 0;
};

CrossCheckReport summarize_bytes(const uint8_t* data, size_t size);
CandidateSignal score_binpack_candidate(const uint8_t* data, size_t size);
CandidateSignal score_minidb_candidate(const uint8_t* data, size_t size);
CandidateSignal score_cfgscript_candidate(const uint8_t* data, size_t size);
CandidateSignal score_streamcodec_candidate(const uint8_t* data, size_t size);
std::string render_crosscheck(const CrossCheckReport& report);

}  // namespace helioforge
