#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "support/util.h"
#include "test_harness.h"

namespace seq_legacy_test {

namespace {

struct Entry {
  const char* name;
  TestFunction function;
};

// Created by the first Registrar, which runs before main().
std::vector<Entry>& Tests() {
  static const std::unique_ptr<std::vector<Entry>> tests(
      new std::vector<Entry>());
  return *tests;
}

int g_failures_in_current_test = 0;

}  // namespace

Registrar::Registrar(const char* name, TestFunction function) {
  Tests().push_back(Entry{name, function});
}

void ReportFailure(const char* file, int line, const std::string& message) {
  ++g_failures_in_current_test;
  std::printf("  %s:%d: %s\n", file, line, message.c_str());
}

std::string ReadFixture(const std::string& relative_path) {
  std::string contents;
  std::string error;
  const std::string path =
      std::string(SEQ_LEGACY_FIXTURE_DIR) + "/" + relative_path;
  if (!seq_legacy::ReadFile(path, &contents, &error)) {
    ReportFailure(__FILE__, __LINE__, error);
    return "";
  }
  // A checkout on Windows may have converted the line endings.
  std::string normalized;
  for (const char c : contents) {
    if (c != '\r') normalized.push_back(c);
  }
  return normalized;
}

}  // namespace seq_legacy_test

// Usage: seq_legacy_unit_tests [name-prefix]
int main(int argc, char* argv[]) {
  const char* prefix = argc > 1 ? argv[1] : "";
  int run = 0;
  int failed = 0;
  for (const auto& test : seq_legacy_test::Tests()) {
    if (std::strncmp(test.name, prefix, std::strlen(prefix)) != 0) continue;
    ++run;
    seq_legacy_test::g_failures_in_current_test = 0;
    test.function();
    if (seq_legacy_test::g_failures_in_current_test != 0) {
      ++failed;
      std::printf("FAILED %s\n", test.name);
    }
  }
  std::printf("%d test(s) run, %d failed\n", run, failed);
  if (run == 0) {
    std::printf("no test matches '%s'\n", prefix);
    return 1;
  }
  return failed == 0 ? 0 : 1;
}
