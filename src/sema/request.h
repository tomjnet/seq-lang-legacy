#ifndef SEQ_LEGACY_SEMA_REQUEST_H_
#define SEQ_LEGACY_SEMA_REQUEST_H_

#include <string>

#include "parser/ast.h"
#include "support/diagnostics.h"
#include "support/source_location.h"

namespace seq_legacy {

constexpr char kCreateFilePattern[] = "create file <file> with <N> <dataset>";
constexpr char kTopFilterPattern[] =
    "for-each transaction get <N> top <filter>";

enum class RequestKind { kCreateFile, kTopFilter };

// What one ask("...") asks for, after its text matched a known pattern.
struct Request {
  RequestKind kind = RequestKind::kCreateFile;
  SourceLocation location;
  std::string file;     // kCreateFile
  std::string builtin;  // dataset or filter name
  int count = 0;
};

enum class RequestStatus {
  // The text matches neither pattern (E0401 was reported).
  kUnrecognized,
  // The pattern is known but a part is malformed (E0404 or E0405 was
  // reported). `kind` is set.
  kInvalid,
  kValid,
};

// Matches the prompt of `ask` against the two request patterns. The words of
// the prompt are separated by single spaces. This checks the shape of each
// part only; whether the dataset or filter exists and whether the count is in
// range is decided by the validator.
RequestStatus ParseRequest(const AskStmt& ask, Diagnostics* diagnostics,
                           Request* request);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SEMA_REQUEST_H_
