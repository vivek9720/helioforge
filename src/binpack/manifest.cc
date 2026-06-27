#include "binpack/manifest.h"

#include <algorithm>
#include <sstream>

namespace helioforge::binpack {
namespace {

size_t count_descendants(const Record& record) {
  size_t count = record.children.size();
  for (const auto& child : record.children) count += count_descendants(child);
  return count;
}

void collect_record_summaries(const Record& record, std::vector<RecordSummary>* out) {
  RecordSummary summary;
  summary.type = record.type;
  summary.id = record.id;
  summary.payload_size = record.payload.size();
  summary.descendant_count = count_descendants(record);
  summary.attributes = record.attributes;
  out->push_back(std::move(summary));
  for (const auto& child : record.children) collect_record_summaries(child, out);
}

void collect_records_of_type(const Record& record, uint16_t type,
                             std::vector<const Record*>* out) {
  if (record.type == type) out->push_back(&record);
  for (const auto& child : record.children) collect_records_of_type(child, type, out);
}

void extend_digest(std::vector<uint8_t>* digest, uint32_t value) {
  digest->push_back(static_cast<uint8_t>(value & 0xffu));
  digest->push_back(static_cast<uint8_t>((value >> 8) & 0xffu));
  digest->push_back(static_cast<uint8_t>((value >> 16) & 0xffu));
  digest->push_back(static_cast<uint8_t>((value >> 24) & 0xffu));
}

void walk_chain(uint16_t start, const std::map<uint16_t, std::set<uint16_t>>& outgoing,
                std::vector<uint16_t>* path, std::set<uint16_t>* active,
                std::vector<std::vector<uint16_t>>* chains) {
  if (!active->insert(start).second) {
    auto it = std::find(path->begin(), path->end(), start);
    if (it != path->end()) chains->push_back(std::vector<uint16_t>(it, path->end()));
    return;
  }
  path->push_back(start);
  auto edge = outgoing.find(start);
  if (edge != outgoing.end()) {
    for (uint16_t next : edge->second) walk_chain(next, outgoing, path, active, chains);
  }
  path->pop_back();
  active->erase(start);
}

}  // namespace

Manifest build_manifest(const Container& container) {
  Manifest manifest;
  manifest.version = container.version;
  manifest.metadata = container.metadata;
  ReferenceGraph graph = build_reference_graph(container);

  std::map<uint16_t, size_t> section_positions;
  for (size_t i = 0; i < container.sections.size(); ++i) {
    const auto& section = container.sections[i];
    section_positions[section.id] = i;
    SectionSummary summary;
    summary.id = section.id;
    summary.kind = section.kind;
    summary.name = section.name;
    summary.payload_size = section.payload.size();
    summary.record_count = section.records.size();
    summary.computed_checksum = checksum32(section.payload.data(), section.payload.size());
    summary.outgoing_refs = section.references;
    auto incoming = graph.incoming.find(section.id);
    if (incoming != graph.incoming.end()) {
      summary.incoming_refs.assign(incoming->second.begin(), incoming->second.end());
    }
    for (const auto& record : section.records) {
      summary.nested_record_count += count_descendants(record);
      collect_record_summaries(record, &manifest.records);
    }
    if (section.declared_checksum != 0 && section.declared_checksum != summary.computed_checksum) {
      manifest.warnings.push_back("section " + std::to_string(section.id) +
                                  " has a checksum mismatch");
    }
    manifest.sections.push_back(std::move(summary));
  }

  for (uint16_t id : container.index_order) {
    if (!section_positions.count(id)) {
      manifest.warnings.push_back("index references missing section " + std::to_string(id));
    }
  }
  for (uint16_t missing : graph.missing) {
    manifest.warnings.push_back("section reference is unresolved: " + std::to_string(missing));
  }
  for (const auto& chain : graph.chains) {
    std::string text = "reference cycle";
    for (uint16_t id : chain) text += " " + std::to_string(id);
    manifest.warnings.push_back(text);
  }
  return manifest;
}

ReferenceGraph build_reference_graph(const Container& container) {
  ReferenceGraph graph;
  std::set<uint16_t> ids;
  for (const auto& section : container.sections) ids.insert(section.id);
  for (const auto& section : container.sections) {
    for (uint16_t ref : section.references) {
      graph.outgoing[section.id].insert(ref);
      graph.incoming[ref].insert(section.id);
      if (!ids.count(ref)) graph.missing.push_back(ref);
    }
  }
  std::sort(graph.missing.begin(), graph.missing.end());
  graph.missing.erase(std::unique(graph.missing.begin(), graph.missing.end()), graph.missing.end());

  std::set<uint16_t> active;
  std::vector<uint16_t> path;
  for (const auto& section : container.sections) {
    walk_chain(section.id, graph.outgoing, &path, &active, &graph.chains);
  }
  std::sort(graph.chains.begin(), graph.chains.end());
  graph.chains.erase(std::unique(graph.chains.begin(), graph.chains.end()), graph.chains.end());
  return graph;
}

std::vector<const Record*> find_records_by_type(const Container& container, uint16_t type) {
  std::vector<const Record*> out;
  for (const auto& section : container.sections) {
    for (const auto& record : section.records) collect_records_of_type(record, type, &out);
  }
  return out;
}

std::vector<uint8_t> canonical_payload_digest(const Container& container) {
  std::vector<const Section*> sections;
  for (const auto& section : container.sections) sections.push_back(&section);
  std::sort(sections.begin(), sections.end(), [](const Section* a, const Section* b) {
    if (a->id != b->id) return a->id < b->id;
    return a->name < b->name;
  });

  std::vector<uint8_t> digest;
  extend_digest(&digest, container.version);
  for (const auto* section : sections) {
    extend_digest(&digest, section->id);
    extend_digest(&digest, section->kind);
    extend_digest(&digest, checksum32(section->payload.data(), section->payload.size()));
    extend_digest(&digest, static_cast<uint32_t>(section->records.size()));
    for (uint16_t ref : section->references) extend_digest(&digest, ref);
  }
  return digest;
}

std::string describe_manifest(const Manifest& manifest) {
  std::ostringstream out;
  out << "version=" << manifest.version << " sections=" << manifest.sections.size()
      << " records=" << manifest.records.size() << " warnings=" << manifest.warnings.size();
  for (const auto& section : manifest.sections) {
    out << "\nsection " << section.id << " kind=" << section.kind << " name=" << section.name
        << " payload=" << section.payload_size << " records=" << section.record_count
        << " nested=" << section.nested_record_count << " refs=" << section.outgoing_refs.size();
  }
  for (const auto& warning : manifest.warnings) out << "\nwarning: " << warning;
  return out.str();
}

}  // namespace helioforge::binpack
