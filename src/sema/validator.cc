#include "sema/validator.h"

#include <map>
#include <utility>

#include "parser/parser.h"
#include "sema/builtins.h"
#include "support/util.h"

namespace seq_legacy {

namespace {

bool IsBlank(StringView text) { return TrimWhitespace(text).empty(); }

void ValidateUnsupported(const Program& program, Diagnostics* diagnostics) {
  for (const UnsupportedDecl& decl : program.unsupported) {
    diagnostics->Error(
        "E0407", decl.location,
        "'" + decl.keyword + "' is not supported in seqc_legacy",
        "remove this line; seqc_legacy has no language model and no backend "
        "selection");
  }
}

void ValidateName(const Program& program, Diagnostics* diagnostics,
                  Workflow* workflow) {
  if (program.names.empty()) {
    diagnostics->Error("E0303", {1, 1}, "missing required name declaration",
                       "add name = \"project_name\"");
    return;
  }
  for (std::size_t i = 1; i < program.names.size(); ++i) {
    diagnostics->Error("E0304", program.names[i].location,
                       "duplicate name declaration",
                       "first declared on line " +
                           std::to_string(program.names[0].location.line));
  }
  const NameDecl& decl = program.names.front();
  std::string reason;
  if (!IsValidProjectName(decl.value, &reason)) {
    diagnostics->Error(decl.value.size() > kMaxNameLength ? "E0313" : "E0312",
                       decl.value_location, "invalid name: " + reason,
                       "use letters, digits, underscores, and hyphens, "
                       "starting with a letter or underscore");
    return;
  }
  workflow->name = decl.value;
}

// Checks a request against the built-ins. `created` says whether an earlier
// request already created a file.
bool ResolveRequest(const Request& request, const AskStmt& ask, bool created,
                    Diagnostics* diagnostics) {
  if (request.kind == RequestKind::kCreateFile) {
    const Dataset* dataset = FindDataset(request.builtin);
    if (dataset == nullptr) {
      diagnostics->Error("E0402", ask.prompt_location,
                         "unknown dataset '" + request.builtin + "'",
                         std::string("known datasets: ") + kSampleDatasetName);
      return false;
    }
    const std::size_t rows = dataset->rows.size();
    if (request.count < 1 || static_cast<std::size_t>(request.count) > rows) {
      diagnostics->Error("E0404", ask.prompt_location,
                         "count " + std::to_string(request.count) +
                             " is out of range: " + dataset->name + " has " +
                             std::to_string(rows) + " rows",
                         "use a count from 1 to " + std::to_string(rows));
      return false;
    }
    return true;
  }

  bool ok = true;
  if (FindFilter(request.builtin) == nullptr) {
    diagnostics->Error("E0403", ask.prompt_location,
                       "unknown filter '" + request.builtin + "'",
                       std::string("known filters: ") + kRevenueFilterName);
    ok = false;
  }
  if (request.count < 1 || request.count > kMaxTopCount) {
    diagnostics->Error(
        "E0404", ask.prompt_location,
        "count " + std::to_string(request.count) + " is out of range",
        "use a count from 1 to " + std::to_string(kMaxTopCount));
    ok = false;
  }
  if (!created) {
    diagnostics->Error(
        "E0406", ask.prompt_location,
        "no transactions to filter: no earlier request created a file",
        std::string("add an earlier request: ") + kCreateFilePattern);
    ok = false;
  }
  return ok;
}

void ValidateSteps(const Program& program, Diagnostics* diagnostics,
                   Workflow* workflow) {
  if (program.steps.empty()) {
    // An empty file is reported through its missing name alone.
    if (!program.names.empty()) {
      diagnostics->Error("E0314", {1, 1}, "workflow has no steps",
                         "add a step block: a line `step step1():` followed "
                         "by an indented `ask(\"...\")`");
    }
    return;
  }
  if (program.steps.size() > kMaxSteps) {
    diagnostics->Error(
        "E0318", program.steps[kMaxSteps].location,
        "too many steps: the limit is " + std::to_string(kMaxSteps));
  }

  std::map<std::string, std::size_t> first_line;
  // Whether a create-file request has been seen, valid or not. An invalid
  // one is not reported a second time through E0406.
  bool created = false;
  for (const StepDecl& step : program.steps) {
    // `first` is the entry for this name, `second` whether it is new.
    const auto entry = first_line.emplace(step.name, step.location.line);
    if (!entry.second) {
      diagnostics->Error(
          "E0315", step.name_location,
          "duplicate step name '" + step.name + "'",
          "first declared on line " + std::to_string(entry.first->second));
    }
    if (step.asks.size() > kMaxAsksPerStep) {
      diagnostics->Error("E0319", step.asks[kMaxAsksPerStep].location,
                         "too many ask() statements in step '" + step.name +
                             "': the limit is " +
                             std::to_string(kMaxAsksPerStep));
    }

    WorkflowStep out;
    out.name = step.name;
    for (const AskStmt& ask : step.asks) {
      if (IsBlank(ask.prompt)) {
        diagnostics->Error("E0316", ask.prompt_location,
                           "ask() prompt must not be empty");
        continue;
      }
      if (ask.prompt.size() > kMaxPromptBytes) {
        diagnostics->Error(
            "E0317", ask.prompt_location,
            "ask() prompt is " + std::to_string(ask.prompt.size()) +
                " bytes: the limit is " + std::to_string(kMaxPromptBytes));
        continue;
      }
      Request request;
      const RequestStatus status = ParseRequest(ask, diagnostics, &request);
      if (status == RequestStatus::kUnrecognized) continue;
      const bool is_create = request.kind == RequestKind::kCreateFile;
      if (status == RequestStatus::kValid &&
          ResolveRequest(request, ask, created, diagnostics)) {
        out.requests.push_back(std::move(request));
      }
      if (is_create) created = true;
    }
    workflow->steps.push_back(std::move(out));
  }
}

}  // namespace

std::size_t Workflow::RequestCount() const {
  std::size_t count = 0;
  for (const WorkflowStep& step : steps) count += step.requests.size();
  return count;
}

bool IsValidProjectName(StringView name, std::string* reason) {
  if (name.empty()) {
    *reason = "the name is empty";
    return false;
  }
  if (name.size() > kMaxNameLength) {
    *reason = "the name is longer than " + std::to_string(kMaxNameLength) +
              " characters";
    return false;
  }
  const char first = name.front();
  if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') ||
        first == '_')) {
    *reason = "the name must start with a letter or underscore";
    return false;
  }
  for (const char c : name) {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) {
      *reason =
          "the name contains a character other than letters, digits, "
          "underscores, and hyphens";
      return false;
    }
  }
  return true;
}

bool Validate(const Program& program, Diagnostics* diagnostics,
              Workflow* workflow) {
  const std::size_t before = diagnostics->items().size();
  *workflow = Workflow();
  ValidateUnsupported(program, diagnostics);
  ValidateName(program, diagnostics, workflow);
  ValidateSteps(program, diagnostics, workflow);
  return diagnostics->items().size() == before;
}

bool RunFrontEnd(StringView raw, std::string display_path, FrontEnd* out,
                 Diagnostics* diagnostics) {
  if (!NormalizeSource(raw, std::move(display_path), &out->source,
                       diagnostics)) {
    return false;
  }
  out->lines = Lex(out->source, diagnostics);
  out->program = Parse(out->lines, diagnostics);
  if (!diagnostics->HasErrors()) {
    return Validate(out->program, diagnostics, &out->workflow);
  }
  // After a syntax error the tree is incomplete. What was parsed is still
  // validated, but "missing" findings would only repeat the syntax errors,
  // so they are dropped.
  Diagnostics semantic;
  Validate(out->program, &semantic, &out->workflow);
  for (const Diagnostic& item : semantic.items()) {
    if (item.code == "E0303" || item.code == "E0314") continue;
    diagnostics->Error(item.code, item.location, item.message, item.hint);
  }
  return false;
}

}  // namespace seq_legacy
