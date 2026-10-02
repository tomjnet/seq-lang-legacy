#ifndef SEQ_LEGACY_PARSER_AST_H_
#define SEQ_LEGACY_PARSER_AST_H_

#include <string>
#include <vector>

#include "support/source_location.h"

namespace seq_legacy {

// The syntax tree, as written. The parser records what the file says without
// judging it; cardinality, support and limits are checked by the validator.

struct NameDecl {
  SourceLocation location;
  std::string value;
  SourceLocation value_location;
};

// A model(...) or backend.X() line. The original language has them;
// seqc_legacy records the line only so that the validator can reject it.
struct UnsupportedDecl {
  SourceLocation location;
  std::string keyword;
};

struct AskStmt {
  SourceLocation location;
  std::string prompt;
  SourceLocation prompt_location;
};

struct StepDecl {
  SourceLocation location;
  std::string name;
  SourceLocation name_location;
  std::vector<AskStmt> asks;
};

struct Program {
  std::vector<NameDecl> names;
  std::vector<UnsupportedDecl> unsupported;
  std::vector<StepDecl> steps;
};

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_PARSER_AST_H_
