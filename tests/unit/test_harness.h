// A minimal test harness for seqc_legacy's own tests. It has no
// dependencies, so the suite builds wherever seqc_legacy builds.

#ifndef SEQ_LEGACY_TESTS_UNIT_TEST_HARNESS_H_
#define SEQ_LEGACY_TESTS_UNIT_TEST_HARNESS_H_

#include <sstream>
#include <string>

namespace seq_legacy_test {

using TestFunction = void (*)();

struct Registrar {
  Registrar(const char* name, TestFunction function);
};

void ReportFailure(const char* file, int line, const std::string& message);

template <typename A, typename B>
void CheckEqual(const char* file, int line, const char* a_text,
                const char* b_text, const A& a, const B& b) {
  if (a == b) return;
  std::ostringstream message;
  message << "expected " << a_text << " == " << b_text << "\n    left:  " << a
          << "\n    right: " << b;
  ReportFailure(file, line, message.str());
}

// Reads a file under tests/fixtures; reports a failure and returns an empty
// string if it cannot be read.
std::string ReadFixture(const std::string& relative_path);

}  // namespace seq_legacy_test

#define SEQ_TEST(name)                                                      \
  static void name();                                                       \
  static const ::seq_legacy_test::Registrar registrar_##name(#name, &name); \
  static void name()

#define CHECK(condition)                                         \
  do {                                                           \
    if (!(condition)) {                                          \
      ::seq_legacy_test::ReportFailure(__FILE__, __LINE__,       \
                                       "expected: " #condition); \
    }                                                            \
  } while (false)

#define CHECK_EQ(a, b) \
  ::seq_legacy_test::CheckEqual(__FILE__, __LINE__, #a, #b, (a), (b))

#endif  // SEQ_LEGACY_TESTS_UNIT_TEST_HARNESS_H_
