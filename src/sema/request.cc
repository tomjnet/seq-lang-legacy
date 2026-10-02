#include "sema/request.h"

#include <cstddef>
#include <vector>

#include "support/string_view.h"

namespace seq_legacy {

namespace {

constexpr std::size_t kMaxFileNameLength = 64;
// A count with more digits than this is out of range for every built-in.
constexpr std::size_t kMaxCountDigits = 9;
constexpr int kHugeCount = 1000000000;

std::vector<StringView> SplitOnSpaces(StringView text) {
  std::vector<StringView> words;
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t end = text.find(' ', start);
    if (end == StringView::npos) end = text.size();
    words.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return words;
}

bool IsAlphaNumeric(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9');
}

bool IsValidFileName(StringView name) {
  if (name.empty() || name.size() > kMaxFileNameLength) return false;
  if (!IsAlphaNumeric(name.front()) && name.front() != '_') return false;
  for (const char c : name) {
    if (!IsAlphaNumeric(c) && c != '_' && c != '.' && c != '-') return false;
  }
  return true;
}

bool ParseCount(StringView word, const AskStmt& ask, Diagnostics* diagnostics,
                int* count) {
  bool digits = !word.empty();
  for (const char c : word) {
    if (c < '0' || c > '9') digits = false;
  }
  if (!digits) {
    diagnostics->Error("E0404", ask.prompt_location,
                       "count '" + std::string(word) + "' is not a number",
                       "write a whole number such as 5");
    return false;
  }
  if (word.size() > kMaxCountDigits) {
    *count = kHugeCount;
    return true;
  }
  int value = 0;
  for (const char c : word) value = value * 10 + (c - '0');
  *count = value;
  return true;
}

RequestStatus Unrecognized(const AskStmt& ask, const char* pattern,
                           Diagnostics* diagnostics) {
  diagnostics->Error("E0401", ask.prompt_location, "unrecognized request",
                     std::string("supported: ") + pattern);
  return RequestStatus::kUnrecognized;
}

}  // namespace

RequestStatus ParseRequest(const AskStmt& ask, Diagnostics* diagnostics,
                           Request* request) {
  const std::vector<StringView> words = SplitOnSpaces(ask.prompt);
  request->location = ask.location;

  if (words[0] == "for-each") {
    // for-each transaction get <N> top <filter>
    if (words.size() != 6 || words[1] != "transaction" || words[2] != "get" ||
        words[4] != "top" || words[5].empty()) {
      return Unrecognized(ask, kTopFilterPattern, diagnostics);
    }
    request->kind = RequestKind::kTopFilter;
    request->builtin = std::string(words[5]);
    if (!ParseCount(words[3], ask, diagnostics, &request->count)) {
      return RequestStatus::kInvalid;
    }
    return RequestStatus::kValid;
  }

  // create file <file> with <N> <dataset>
  if (words.size() != 6 || words[0] != "create" || words[1] != "file" ||
      words[3] != "with" || words[5].empty()) {
    return Unrecognized(ask, kCreateFilePattern, diagnostics);
  }
  request->kind = RequestKind::kCreateFile;
  request->file = std::string(words[2]);
  request->builtin = std::string(words[5]);
  bool ok = true;
  if (!IsValidFileName(words[2])) {
    diagnostics->Error(
        "E0405", ask.prompt_location,
        "invalid file name '" + std::string(words[2]) + "'",
        "use letters, digits, '_', '.' and '-', at most 64 characters, "
        "starting with a letter, digit or '_'");
    ok = false;
  }
  if (!ParseCount(words[4], ask, diagnostics, &request->count)) ok = false;
  return ok ? RequestStatus::kValid : RequestStatus::kInvalid;
}

}  // namespace seq_legacy
