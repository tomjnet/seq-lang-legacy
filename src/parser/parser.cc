#include "parser/parser.h"

#include <cstddef>
#include <utility>

#include "support/util.h"

namespace seq_legacy {

namespace {

constexpr char kNameForm[] = "write name = \"project_name\"";
constexpr char kStepForm[] = "write a step header as: step step_name():";
constexpr char kAskForm[] = "write ask(\"...\")";
constexpr char kFileForm[] = "a workflow has name = \"...\" and step blocks";

// Walks the tokens of one line.
class Cursor {
 public:
  Cursor(const SourceLine& line, Diagnostics* diagnostics)
      : line_(line), diagnostics_(diagnostics) {}

  bool AtEnd() const { return index_ >= line_.tokens.size(); }
  const Token& Peek() const { return line_.tokens[index_]; }
  bool Is(TokenKind kind) const { return !AtEnd() && Peek().kind == kind; }
  bool IsWord(const char* word) const {
    return Is(TokenKind::kIdentifier) && Peek().text == word;
  }
  const Token& Next() { return line_.tokens[index_++]; }

  // Location of the current token, or just past the line's last token.
  SourceLocation Here() const { return AtEnd() ? line_.end : Peek().location; }

  std::string Describe() const {
    if (AtEnd()) return "end of line";
    if (Peek().kind == TokenKind::kString) return "a string";
    return "'" + Peek().text + "'";
  }

  // Reports "expected <what>, found <token>" at the current position.
  bool Expected(const std::string& what, const char* hint = "") {
    diagnostics_->Error("E0201", Here(),
                        "expected " + what + ", found " + Describe(), hint);
    return false;
  }

  bool Accept(TokenKind kind) {
    if (!Is(kind)) return false;
    ++index_;
    return true;
  }

  bool ExpectEnd(const char* hint = "") {
    if (AtEnd()) return true;
    return Expected("end of line", hint);
  }

  Diagnostics* diagnostics() { return diagnostics_; }

 private:
  const SourceLine& line_;
  Diagnostics* diagnostics_;
  std::size_t index_ = 0;
};

bool ParseName(Cursor* cursor, SourceLocation location, Program* program) {
  if (!cursor->Accept(TokenKind::kEquals)) {
    return cursor->Expected("'=' after 'name'", kNameForm);
  }
  if (!cursor->Is(TokenKind::kString)) {
    return cursor->Expected("a string", kNameForm);
  }
  NameDecl decl;
  decl.location = location;
  decl.value_location = cursor->Here();
  decl.value = cursor->Next().text;
  if (!cursor->ExpectEnd(kNameForm)) return false;
  program->names.push_back(std::move(decl));
  return true;
}

bool ParseStep(Cursor* cursor, SourceLocation location, Program* program) {
  if (!cursor->Is(TokenKind::kIdentifier)) {
    return cursor->Expected("a step name after 'step'", kStepForm);
  }
  StepDecl decl;
  decl.location = location;
  decl.name_location = cursor->Here();
  decl.name = cursor->Next().text;
  if (IsReservedWord(decl.name)) {
    cursor->diagnostics()->Error(
        "E0208", decl.name_location,
        "'" + decl.name + "' is a reserved word and cannot be a step name",
        "reserved words are model, backend, name, step, and ask");
    return false;
  }
  if (!cursor->Accept(TokenKind::kLeftParen)) {
    return cursor->Expected("'(' after the step name", kStepForm);
  }
  if (!cursor->Is(TokenKind::kRightParen)) {
    if (cursor->AtEnd()) return cursor->Expected("')'", kStepForm);
    cursor->diagnostics()->Error("E0211", cursor->Here(),
                                 "steps take no parameters", kStepForm);
    return false;
  }
  cursor->Next();
  if (!cursor->Accept(TokenKind::kColon)) {
    return cursor->Expected("':' after the step header", kStepForm);
  }
  if (!cursor->ExpectEnd(
          "put each ask() on its own line, indented four spaces")) {
    return false;
  }
  program->steps.push_back(std::move(decl));
  return true;
}

bool ParseAsk(Cursor* cursor, StepDecl* step) {
  if (!cursor->IsWord("ask")) {
    cursor->diagnostics()->Error(
        "E0212", cursor->Here(),
        "only ask(\"...\") statements are allowed inside a step", kAskForm);
    return false;
  }
  AskStmt ask;
  ask.location = cursor->Next().location;
  if (cursor->Is(TokenKind::kString)) {
    cursor->diagnostics()->Error("E0204", cursor->Here(),
                                 "ask requires parentheses", kAskForm);
    return false;
  }
  if (!cursor->Accept(TokenKind::kLeftParen)) {
    return cursor->Expected("'(' after 'ask'", kAskForm);
  }
  if (!cursor->Is(TokenKind::kString)) {
    cursor->diagnostics()->Error(
        "E0213", cursor->Here(),
        "ask() takes exactly one string literal, found " + cursor->Describe(),
        kAskForm);
    return false;
  }
  ask.prompt_location = cursor->Here();
  ask.prompt = cursor->Next().text;
  if (cursor->Is(TokenKind::kComma)) {
    cursor->diagnostics()->Error("E0213", cursor->Here(),
                                 "ask() takes exactly one string literal",
                                 kAskForm);
    return false;
  }
  if (!cursor->Accept(TokenKind::kRightParen)) {
    return cursor->Expected("')'", kAskForm);
  }
  if (!cursor->ExpectEnd("write one ask() per line")) return false;
  step->asks.push_back(std::move(ask));
  return true;
}

std::string At(SourceLocation location) {
  return " @" + std::to_string(location.line) + ":" +
         std::to_string(location.column);
}

}  // namespace

Program Parse(const std::vector<SourceLine>& lines, Diagnostics* diagnostics) {
  Program program;
  // Whether a step is open for indented ask() lines.
  bool step_open = false;
  // The last top-level line was in error; indented lines after it are not
  // reported again as stray indentation.
  bool after_broken_line = false;
  // Steps whose body had an error; they are not also reported as empty.
  std::vector<bool> body_had_error;
  bool seen_step = false;

  for (const SourceLine& line : lines) {
    if (line.indent != 0 && line.indent != 4) {
      if (!line.has_error) {
        diagnostics->Error(
            "E0108", {line.number, line.indent + 1},
            "inconsistent indentation: expected 0 or 4 spaces, found " +
                std::to_string(line.indent),
            "top-level declarations are not indented; ask() statements are "
            "indented exactly four spaces");
      }
      if (step_open) body_had_error.back() = true;
      continue;
    }

    if (line.indent == 4) {
      if (!step_open) {
        if (!after_broken_line && !line.has_error) {
          diagnostics->Error(
              "E0207", {line.number, line.indent + 1}, "unexpected indentation",
              "only ask() statements inside a step are indented");
        }
        continue;
      }
      if (line.has_error) {
        body_had_error.back() = true;
        continue;
      }
      Cursor cursor(line, diagnostics);
      if (!ParseAsk(&cursor, &program.steps.back())) {
        body_had_error.back() = true;
      }
      continue;
    }

    // A top-level line closes any open step.
    step_open = false;
    after_broken_line = false;
    if (line.has_error || line.tokens.empty()) {
      after_broken_line = true;
      continue;
    }

    Cursor cursor(line, diagnostics);
    if (!cursor.Is(TokenKind::kIdentifier)) {
      cursor.Expected("a declaration", kFileForm);
      after_broken_line = true;
      continue;
    }
    const Token keyword = cursor.Next();
    if (keyword.text == "name" && seen_step) {
      diagnostics->Error("E0205", keyword.location,
                         "'name' must be declared before the first step",
                         "move the name declaration to the top of the file");
    }

    bool ok = false;
    if (keyword.text == "model" || keyword.text == "backend") {
      // The rest of the line is not parsed: the whole declaration is rejected
      // by the validator.
      program.unsupported.push_back(
          UnsupportedDecl{keyword.location, keyword.text});
      ok = true;
    } else if (keyword.text == "name") {
      ok = ParseName(&cursor, keyword.location, &program);
    } else if (keyword.text == "step") {
      seen_step = true;
      ok = ParseStep(&cursor, keyword.location, &program);
      if (ok) {
        step_open = true;
        body_had_error.push_back(false);
      }
    } else if (keyword.text == "ask") {
      diagnostics->Error("E0206", keyword.location,
                         "ask() must be inside a step",
                         "indent it four spaces under a step header");
    } else {
      diagnostics->Error("E0201", keyword.location,
                         "unknown declaration '" + keyword.text + "'",
                         kFileForm);
    }
    if (!ok) after_broken_line = true;
  }

  for (std::size_t i = 0; i < program.steps.size(); ++i) {
    if (program.steps[i].asks.empty() && !body_had_error[i]) {
      diagnostics->Error(
          "E0209", program.steps[i].location,
          "step '" + program.steps[i].name + "' has no ask() statements",
          "add at least one indented ask(\"...\") line");
    }
  }
  return program;
}

std::string AstToText(const Program& program) {
  std::string out = "program\n";
  for (const NameDecl& name : program.names) {
    out += "  name " + Quote(name.value) + At(name.location) + "\n";
  }
  for (const UnsupportedDecl& decl : program.unsupported) {
    out += "  unsupported " + decl.keyword + At(decl.location) + "\n";
  }
  for (const StepDecl& step : program.steps) {
    out += "  step " + step.name + At(step.location) + "\n";
    for (const AskStmt& ask : step.asks) {
      out += "    ask " + Quote(ask.prompt) + At(ask.location) + "\n";
    }
  }
  return out;
}

}  // namespace seq_legacy
