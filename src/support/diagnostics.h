#ifndef SEQ_LEGACY_SUPPORT_DIAGNOSTICS_H_
#define SEQ_LEGACY_SUPPORT_DIAGNOSTICS_H_

#include <ostream>
#include <string>
#include <vector>

#include "support/source_location.h"

namespace seq_legacy {

struct Diagnostic {
  // Stable code such as "E0304". lists the E04xx codes.
  std::string code;
  SourceLocation location;
  std::string message;
  // Optional second line that says how to fix the problem.
  std::string hint;
};

class Diagnostics {
 public:
  void Error(std::string code, SourceLocation location, std::string message,
             std::string hint = "");

  bool HasErrors() const { return !items_.empty(); }
  const std::vector<Diagnostic>& items() const { return items_; }

  // Prints every diagnostic, ordered by position, as
  //   path:line:column: error[E0000]: message
  // followed by the source line, a caret and the hint, and then one summary
  // line with the number of errors.
  void Print(std::ostream& out, const SourceText& source) const;

 private:
  std::vector<Diagnostic> items_;
};

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SUPPORT_DIAGNOSTICS_H_
