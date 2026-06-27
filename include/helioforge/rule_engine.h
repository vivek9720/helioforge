#pragma once

#include "helioforge/byte_tools.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace helioforge {

enum class RuleSeverity {
  kInfo,
  kWarning,
  kError
};

struct RuleSpec {
  std::string family;
  std::string name;
  std::string field;
  RuleSeverity severity = RuleSeverity::kInfo;
  size_t min_size = 0;
  size_t max_size = 0;
  std::string marker;
  bool requires_text = false;
  uint32_t opcode = 0;
  std::string description;
};

struct RuleHit {
  std::string family;
  std::string name;
  RuleSeverity severity = RuleSeverity::kInfo;
  size_t confidence = 0;
  std::string detail;
};

class RuleEngine {
 public:
  explicit RuleEngine(std::vector<RuleSpec> rules);
  std::vector<RuleHit> evaluate(const uint8_t* data, size_t size) const;
  const std::vector<RuleSpec>& rules() const { return rules_; }

 private:
  std::vector<RuleSpec> rules_;
};

const std::vector<RuleSpec>& default_rule_catalog();
std::vector<RuleHit> evaluate_default_rules(const uint8_t* data, size_t size);
std::string severity_name(RuleSeverity severity);
std::string render_rule_hits(const std::vector<RuleHit>& hits, size_t limit);

}  // namespace helioforge
