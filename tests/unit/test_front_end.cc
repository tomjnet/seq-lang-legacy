// Lexer, parser and semantic analysis.

#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "project/project.h"
#include "sema/builtins.h"
#include "sema/request.h"
#include "sema/validator.h"
#include "support/diagnostics.h"
#include "support/filesystem.h"
#include "test_harness.h"

namespace {

using seq_legacy::Diagnostics;
using seq_legacy::FrontEnd;
using seq_legacy::RequestKind;
using seq_legacy::SourceLine;
using seq_legacy::TokenKind;
using seq_legacy_test::ReadFixture;

// Runs the front end on `source` and returns the diagnostics as printed for
// a file called FILE; empty if the source is valid.
std::string Check(const std::string& source, FrontEnd* front_end = nullptr) {
  FrontEnd local;
  if (front_end == nullptr) front_end = &local;
  Diagnostics diagnostics;
  if (seq_legacy::RunFrontEnd(source, "FILE", front_end, &diagnostics)) {
    return "";
  }
  std::ostringstream out;
  diagnostics.Print(out, front_end->source);
  return out.str();
}

// Whether `source` is rejected with exactly one error, with code `code`.
bool FailsWith(const std::string& source, const std::string& code) {
  FrontEnd front_end;
  Diagnostics diagnostics;
  if (seq_legacy::RunFrontEnd(source, "FILE", &front_end, &diagnostics)) {
    return false;
  }
  return diagnostics.items().size() == 1 && diagnostics.items()[0].code == code;
}

std::string Program(const std::string& first_ask,
                    const std::string& second_ask = "") {
  std::string source =
      "name = \"demo\"\n\nstep step1():\n    ask(\"" + first_ask + "\")\n";
  if (!second_ask.empty()) {
    source += "\nstep step2():\n    ask(\"" + second_ask + "\")\n";
  }
  return source;
}

// The names, without ".seq", of the sources in fixtures/language/invalid.
std::vector<std::string> InvalidFixtures() {
  const std::string suffix = ".seq";
  std::vector<std::string> files;
  CHECK(seq_legacy::ListDirectory(
      std::string(SEQ_LEGACY_FIXTURE_DIR) + "/language/invalid", &files));
  std::vector<std::string> names;
  for (const std::string& file : files) {
    if (file.size() <= suffix.size()) continue;
    const std::size_t stem = file.size() - suffix.size();
    if (file.compare(stem, suffix.size(), suffix) == 0) {
      names.push_back(file.substr(0, stem));
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

}  // namespace

SEQ_TEST(LexerProducesTheOriginalTokenSet) {
  seq_legacy::SourceText source;
  Diagnostics diagnostics;
  CHECK(seq_legacy::NormalizeSource("step a(): x = \"s\" 5 . ,\n", "FILE",
                                    &source, &diagnostics));
  const std::vector<SourceLine> lines = seq_legacy::Lex(source, &diagnostics);
  CHECK(!diagnostics.HasErrors());
  CHECK_EQ(lines.size(), 1u);
  const std::vector<TokenKind> expected = {
      TokenKind::kIdentifier, TokenKind::kIdentifier, TokenKind::kLeftParen,
      TokenKind::kRightParen, TokenKind::kColon,      TokenKind::kIdentifier,
      TokenKind::kEquals,     TokenKind::kString,     TokenKind::kNumber,
      TokenKind::kDot,        TokenKind::kComma};
  CHECK_EQ(lines[0].tokens.size(), expected.size());
  for (std::size_t i = 0; i < expected.size(); ++i) {
    CHECK(lines[0].tokens[i].kind == expected[i]);
  }
}

SEQ_TEST(LexerReportsIndentAndSkipsBlankAndCommentLines) {
  seq_legacy::SourceText source;
  Diagnostics diagnostics;
  CHECK(seq_legacy::NormalizeSource(
      "# comment\n\nstep a():   # trailing\n    ask(\"x # not a comment\")\n",
      "FILE", &source, &diagnostics));
  const std::vector<SourceLine> lines = seq_legacy::Lex(source, &diagnostics);
  CHECK_EQ(lines.size(), 2u);
  CHECK_EQ(lines[0].number, 3u);
  CHECK_EQ(lines[0].indent, 0u);
  CHECK_EQ(lines[0].tokens.size(), 5u);
  CHECK_EQ(lines[1].indent, 4u);
  CHECK_EQ(lines[1].tokens[2].text, std::string("x # not a comment"));
  CHECK_EQ(lines[1].tokens[2].location.column, 9u);
}

SEQ_TEST(LexerDecodesEscapes) {
  seq_legacy::SourceText source;
  Diagnostics diagnostics;
  CHECK(seq_legacy::NormalizeSource("name = \"a\\\"b\\\\c\\nd\\te\"\n", "FILE",
                                    &source, &diagnostics));
  const std::vector<SourceLine> lines = seq_legacy::Lex(source, &diagnostics);
  CHECK(!diagnostics.HasErrors());
  CHECK_EQ(lines[0].tokens[2].text, std::string("a\"b\\c\nd\te"));
}

SEQ_TEST(LexerAcceptsBomAndCrlf) {
  FrontEnd front_end;
  const std::string source =
      "\xEF\xBB\xBFname = \"demo\"\r\n\r\nstep step1():\r\n"
      "    ask(\"create file a.txt with 1 sample-company-transaction-db\")\r\n";
  CHECK_EQ(Check(source, &front_end), std::string(""));
  CHECK_EQ(front_end.workflow.name, std::string("demo"));
}

SEQ_TEST(LexerRejectsTabsAndBadEscapes) {
  CHECK(FailsWith("name = \"demo\"\nstep a():\n\task(\"x\")\n", "E0103"));
  CHECK(FailsWith("name = \"de\\qmo\"\n", "E0106"));
  CHECK(FailsWith("name = \"demo\n", "E0105"));
  CHECK(FailsWith("name = \"demo\" @\n", "E0107"));
}

SEQ_TEST(ParserBuildsTheReferenceProgram) {
  FrontEnd front_end;
  CHECK_EQ(Check(ReadFixture("language/valid/reference.seq"), &front_end),
           std::string(""));
  const seq_legacy::Program& program = front_end.program;
  CHECK_EQ(program.names.size(), 1u);
  CHECK_EQ(program.names[0].value, std::string("top3Company"));
  CHECK_EQ(program.steps.size(), 2u);
  CHECK_EQ(program.steps[0].name, std::string("step1"));
  CHECK_EQ(program.steps[0].asks.size(), 1u);
  CHECK_EQ(program.steps[1].asks[0].prompt,
           std::string("for-each transaction get 3 top "
                       "total-revenue-by-company"));
  CHECK_EQ(program.steps[1].asks[0].location.line, 7u);
  CHECK_EQ(program.steps[1].asks[0].location.column, 5u);
}

SEQ_TEST(StarterSourceIsTheReferenceProgram) {
  CHECK_EQ(seq_legacy::StarterSource("top3Company"),
           ReadFixture("language/valid/reference.seq"));
  CHECK_EQ(seq_legacy::StarterSource("top3Company"),
           ReadFixture("../../examples/top3Company/src/main.seq"));
}

SEQ_TEST(ValidatorResolvesTheReferenceRequests) {
  FrontEnd front_end;
  CHECK_EQ(Check(ReadFixture("language/valid/reference.seq"), &front_end),
           std::string(""));
  const seq_legacy::Workflow& workflow = front_end.workflow;
  CHECK_EQ(workflow.name, std::string("top3Company"));
  CHECK_EQ(workflow.steps.size(), 2u);
  CHECK_EQ(workflow.RequestCount(), 2u);

  const seq_legacy::Request& create = workflow.steps[0].requests[0];
  CHECK(create.kind == RequestKind::kCreateFile);
  CHECK_EQ(create.file, std::string("company.txt"));
  CHECK_EQ(create.count, 5);
  CHECK_EQ(create.builtin, std::string(seq_legacy::kSampleDatasetName));

  const seq_legacy::Request& top = workflow.steps[1].requests[0];
  CHECK(top.kind == RequestKind::kTopFilter);
  CHECK_EQ(top.count, 3);
  CHECK_EQ(top.builtin, std::string(seq_legacy::kRevenueFilterName));
}

SEQ_TEST(ValidatorAcceptsCommentsEscapesAndSeveralAsks) {
  FrontEnd front_end;
  CHECK_EQ(Check(ReadFixture("language/valid/comments_and_several_asks.seq"),
                 &front_end),
           std::string(""));
  CHECK_EQ(front_end.workflow.name, std::string("notes_demo-1"));
  CHECK_EQ(front_end.workflow.steps.size(), 2u);
  CHECK_EQ(front_end.workflow.RequestCount(), 4u);
}

SEQ_TEST(RequestCountBoundaries) {
  const std::string dataset = " sample-company-transaction-db";
  const std::string filter = " top total-revenue-by-company";
  const std::string create = "create file a.txt with 5" + dataset;

  CHECK_EQ(Check(Program("create file a.txt with 1" + dataset)),
           std::string(""));
  CHECK_EQ(Check(Program("create file a.txt with 500" + dataset)),
           std::string(""));
  CHECK(FailsWith(Program("create file a.txt with 0" + dataset), "E0404"));
  CHECK(FailsWith(Program("create file a.txt with 501" + dataset), "E0404"));
  CHECK(FailsWith(Program("create file a.txt with five" + dataset), "E0404"));
  CHECK(FailsWith(Program("create file a.txt with 99999999999" + dataset),
                  "E0404"));

  CHECK_EQ(Check(Program(create, "for-each transaction get 1" + filter)),
           std::string(""));
  CHECK_EQ(Check(Program(create, "for-each transaction get 100" + filter)),
           std::string(""));
  CHECK(FailsWith(Program(create, "for-each transaction get 0" + filter),
                  "E0404"));
  CHECK(FailsWith(Program(create, "for-each transaction get 101" + filter),
                  "E0404"));
}

SEQ_TEST(RequestPatternsMustMatchExactly) {
  const std::string create =
      "create file a.txt with 5 sample-company-transaction-db";
  // Two spaces, a missing word, an extra word, different case.
  CHECK(FailsWith(Program("create  file a.txt with 5 "
                          "sample-company-transaction-db"),
                  "E0401"));
  CHECK(FailsWith(Program("create a.txt with 5 sample-company-transaction-db"),
                  "E0401"));
  CHECK(FailsWith(Program(create + " please"), "E0401"));
  CHECK(FailsWith(Program("Create file a.txt with 5 "
                          "sample-company-transaction-db"),
                  "E0401"));
  CHECK(FailsWith(Program(create,
                          "for-each row get 3 top "
                          "total-revenue-by-company"),
                  "E0401"));
  CHECK(FailsWith(Program(create,
                          "for-each transaction get 3 best "
                          "total-revenue-by-company"),
                  "E0401"));
}

SEQ_TEST(RequestFileNames) {
  const std::string tail = " with 5 sample-company-transaction-db";
  CHECK_EQ(Check(Program("create file _a-b.c_9.txt" + tail)), std::string(""));
  CHECK(FailsWith(Program("create file .hidden" + tail), "E0405"));
  CHECK(FailsWith(Program("create file a/b.txt" + tail), "E0405"));
  CHECK(FailsWith(Program("create file ..\\\\b.txt" + tail), "E0405"));
  CHECK(FailsWith(Program("create file " + std::string(65, 'a') + tail),
                  "E0405"));
  CHECK_EQ(Check(Program("create file " + std::string(64, 'a') + tail)),
           std::string(""));
}

SEQ_TEST(ValidatorChecksNameAndSteps) {
  const std::string ask =
      "    ask(\"create file a.txt with 5 sample-company-transaction-db\")\n";
  CHECK(FailsWith("step a():\n" + ask, "E0303"));
  CHECK(FailsWith("name = \"a\"\nname = \"b\"\nstep a():\n" + ask, "E0304"));
  CHECK(FailsWith("name = \"9lives\"\nstep a():\n" + ask, "E0312"));
  CHECK(FailsWith("name = \"" + std::string(65, 'a') + "\"\nstep a():\n" + ask,
                  "E0313"));
  CHECK(FailsWith("name = \"a\"\n", "E0314"));
  CHECK(FailsWith("name = \"a\"\nstep a():\n" + ask + "step a():\n" + ask,
                  "E0315"));
  CHECK(FailsWith("name = \"a\"\nstep a():\n    ask(\"  \")\n", "E0316"));
  CHECK(FailsWith("", "E0303"));
}

SEQ_TEST(InvalidFixturesMatchTheirGoldenDiagnostics) {
  const std::vector<std::string> names = InvalidFixtures();
  CHECK(names.size() >= 10u);
  for (const std::string& name : names) {
    const std::string source = ReadFixture("language/invalid/" + name + ".seq");
    const std::string expected =
        ReadFixture("language/invalid/" + name + ".expected");
    const std::string actual = Check(source);
    if (actual != expected) {
      seq_legacy_test::ReportFailure(__FILE__, __LINE__,
                                     "fixture " + name + ": expected\n" +
                                         expected + "\nactual\n" + actual);
    }
  }
}

SEQ_TEST(EveryRequestCodeHasAFixture) {
  std::string all;
  for (const std::string& name : InvalidFixtures()) {
    all += ReadFixture("language/invalid/" + name + ".expected");
  }
  for (const char* code :
       {"E0401", "E0402", "E0403", "E0404", "E0405", "E0406", "E0407"}) {
    if (all.find(std::string("error[") + code + "]") == std::string::npos) {
      seq_legacy_test::ReportFailure(__FILE__, __LINE__,
                                     std::string("no fixture reports ") + code);
    }
  }
}

SEQ_TEST(OneRunReportsSeveralErrors) {
  FrontEnd front_end;
  Diagnostics diagnostics;
  CHECK(!seq_legacy::RunFrontEnd(
      ReadFixture("language/invalid/several_errors.seq"), "FILE", &front_end,
      &diagnostics));
  CHECK(diagnostics.items().size() >= 5u);
}
