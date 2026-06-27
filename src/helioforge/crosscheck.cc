#include "helioforge/crosscheck.h"

#include "binpack/binpack.h"
#include "binpack/manifest.h"
#include "cfgscript/cfgscript.h"
#include "cfgscript/evaluator.h"
#include "minidb/minidb.h"
#include "minidb/query.h"
#include "helioforge/schema.h"
#include "streamcodec/session.h"
#include "streamcodec/streamcodec.h"

#include <algorithm>
#include <sstream>

namespace helioforge {
namespace {

void add_note(CandidateSignal* signal, std::string note, size_t confidence) {
  signal->notes.push_back(std::move(note));
  signal->confidence += confidence;
}

bool has_prefix(const uint8_t* data, size_t size, const char* prefix, size_t prefix_size) {
  return size >= prefix_size &&
         std::equal(reinterpret_cast<const uint8_t*>(prefix),
                    reinterpret_cast<const uint8_t*>(prefix) + prefix_size, data);
}

}  // namespace

CandidateSignal score_binpack_candidate(const uint8_t* data, size_t size) {
  CandidateSignal signal;
  signal.subsystem = "binpack";
  if (has_prefix(data, size, "BPK1", 4)) add_note(&signal, "magic header present", 20);
  auto hits = find_marker(data, size, "BPK1");
  if (hits.size() > 1) add_note(&signal, "multiple embedded containers", hits.size());
  bool ok = false;
  uint16_t sections = read_le16(data, size, 8, &ok);
  if (ok && sections > 0 && sections < 128) add_note(&signal, "plausible section count", 6);
  if (size > 16 && shannon_entropy(data, std::min<size_t>(size, 128)) > 2.0) {
    add_note(&signal, "binary-like header entropy", 2);
  }
  return signal;
}

CandidateSignal score_minidb_candidate(const uint8_t* data, size_t size) {
  CandidateSignal signal;
  signal.subsystem = "minidb";
  if (has_prefix(data, size, "MDB1", 4)) add_note(&signal, "magic header present", 20);
  bool ok = false;
  uint16_t page_size = read_le16(data, size, 4, &ok);
  if (ok && page_size >= 64 && page_size <= 65535) add_note(&signal, "plausible page size", 5);
  auto first_var = read_varint(data, size, 10);
  if (first_var.ok && first_var.width <= 5) add_note(&signal, "decodable early varint", 3);
  if (find_marker(data, size, "MDB1").size() > 1) add_note(&signal, "embedded database marker", 1);
  return signal;
}

CandidateSignal score_cfgscript_candidate(const uint8_t* data, size_t size) {
  CandidateSignal signal;
  signal.subsystem = "cfgscript";
  if (looks_like_text(data, size)) add_note(&signal, "mostly textual input", 10);
  for (const char* word : {"include", "true", "false", "$", "{", "[", "="}) {
    auto hits = find_marker(data, size, word);
    if (!hits.empty()) add_note(&signal, std::string("token ") + word, std::min<size_t>(hits.size(), 4));
  }
  return signal;
}

CandidateSignal score_streamcodec_candidate(const uint8_t* data, size_t size) {
  CandidateSignal signal;
  signal.subsystem = "streamcodec";
  auto hits = find_marker(data, size, "SC");
  if (!hits.empty()) add_note(&signal, "frame marker present", 8 + hits.size());
  for (size_t hit : hits) {
    if (hit + 17 <= size) {
      bool ok_meta = false;
      bool ok_payload = false;
      uint16_t meta = read_le16(data, size, hit + 12, &ok_meta);
      uint16_t payload = read_le16(data, size, hit + 14, &ok_payload);
      if (ok_meta && ok_payload && hit + 17u + meta + payload <= size) {
        add_note(&signal, "complete frame boundary", 10);
      }
    }
  }
  return signal;
}

CrossCheckReport summarize_bytes(const uint8_t* data, size_t size) {
  CrossCheckReport report;
  report.profile = profile_bytes(data, size, 64);
  SchemaGuess schema = looks_like_text(data, size) ? infer_text_schema(data, size)
                                                  : infer_binary_schema(data, size);
  report.inferred_fields = schema.fields.size();
  if (!schema.diagnostics.empty()) {
    report.parser_notes.insert(report.parser_notes.end(), schema.diagnostics.begin(),
                               schema.diagnostics.end());
  }
  report.signals.push_back(score_binpack_candidate(data, size));
  report.signals.push_back(score_minidb_candidate(data, size));
  report.signals.push_back(score_cfgscript_candidate(data, size));
  report.signals.push_back(score_streamcodec_candidate(data, size));
  std::sort(report.signals.begin(), report.signals.end(),
            [](const CandidateSignal& a, const CandidateSignal& b) {
              if (a.confidence != b.confidence) return a.confidence > b.confidence;
              return a.subsystem < b.subsystem;
            });

  auto bp = binpack::parse(data, size);
  report.parser_error_count += bp.errors.size();
  if (!bp.value.sections.empty()) {
    auto manifest = binpack::build_manifest(bp.value);
    report.reconstructed_items += manifest.sections.size() + manifest.records.size();
    report.parser_notes.push_back("binpack sections=" + std::to_string(manifest.sections.size()));
  }

  auto db = minidb::decode(data, size);
  report.parser_error_count += db.errors.size();
  if (!db.value.pages.empty() || !db.value.visible_records.empty()) {
    auto stats = minidb::analyze_store(db.value);
    report.reconstructed_items += stats.page_count + stats.visible_record_count;
    report.parser_notes.push_back("minidb pages=" + std::to_string(stats.page_count));
  }

  auto cfg = cfgscript::parse(data, size);
  report.parser_error_count += cfg.errors.size();
  if (!cfg.value.globals.empty() || !cfg.value.includes.empty()) {
    auto eval = cfgscript::evaluate_document(cfg.value);
    report.reconstructed_items += eval.resolved.size() + eval.include_names.size();
    report.parser_notes.push_back("cfgscript globals=" + std::to_string(cfg.value.globals.size()));
  }

  auto stream = streamcodec::analyze_stream_chunks(data, size, 13);
  report.parser_error_count += stream.errors.size();
  if (stream.completed_messages != 0) {
    report.reconstructed_items += stream.completed_messages;
    report.parser_notes.push_back("stream messages=" + std::to_string(stream.completed_messages));
  }
  return report;
}

std::string render_crosscheck(const CrossCheckReport& report) {
  std::ostringstream out;
  out << "size=" << report.profile.size << " ascii=" << report.profile.ascii_bytes
      << " high=" << report.profile.high_bytes << " nul=" << report.profile.nul_bytes
      << " windows=" << report.profile.windows.size()
      << " parser_errors=" << report.parser_error_count
      << " reconstructed=" << report.reconstructed_items
      << " inferred_fields=" << report.inferred_fields;
  for (const auto& signal : report.signals) {
    out << "\n" << signal.subsystem << " confidence=" << signal.confidence;
    for (const auto& note : signal.notes) out << " [" << note << "]";
  }
  for (const auto& note : report.parser_notes) out << "\nparser: " << note;
  return out.str();
}

}  // namespace helioforge
