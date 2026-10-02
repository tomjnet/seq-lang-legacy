#ifndef SEQ_LEGACY_SEMA_VALIDATOR_H_
#define SEQ_LEGACY_SEMA_VALIDATOR_H_

#include <cstddef>
#include <string>
#include <vector>

#include "lexer/lexer.h"
#include "parser/ast.h"
#include "sema/request.h"
#include "support/diagnostics.h"
#include "support/source_location.h"
#include "support/string_view.h"

namespace seq_legacy {

// The validated program: what the back end compiles.
struct WorkflowStep {
  std::string name;
  // Requests in source order.
  std::vector<Request> requests;
};

struct Workflow {
  std::string name;
  // Steps in source order.
  std::vector<WorkflowStep> steps;

  std::size_t RequestCount() const;
};

// Everything the front end produces for one source file.
struct FrontEnd {
  SourceText source;
  std::vector<SourceLine> lines;
  Program program;
  Workflow workflow;
};

// True if `name` is [A-Za-z_][A-Za-z0-9_-]* and at most 64 characters;
// otherwise `reason` says what is wrong.
bool IsValidProjectName(StringView name, std::string* reason);

// Checks the tree and resolves every ask() to a request. Returns true if it
// reported nothing.
bool Validate(const Program& program, Diagnostics* diagnostics,
              Workflow* workflow);

// Stages 1 to 3: normalize, lex, parse, validate. Returns true if the source
// is a valid workflow; otherwise `diagnostics` holds every error found.
bool RunFrontEnd(StringView raw, std::string display_path, FrontEnd* out,
                 Diagnostics* diagnostics);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SEMA_VALIDATOR_H_
