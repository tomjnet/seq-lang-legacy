#ifndef SEQ_LEGACY_SUPPORT_SOURCE_LOCATION_H_
#define SEQ_LEGACY_SUPPORT_SOURCE_LOCATION_H_

#include <cstddef>
#include <string>

#include "support/string_view.h"

namespace seq_legacy {

// 1-based position in a source file. Columns count Unicode code points.
struct SourceLocation {
  SourceLocation() = default;
  SourceLocation(std::size_t line, std::size_t column)
      : line(line), column(column) {}

  std::size_t line = 1;
  std::size_t column = 1;
};

// Source text after normalization: no byte-order mark, LF line endings.
struct SourceText {
  // Path as the user wrote it, used in diagnostics.
  std::string path;
  std::string text;

  // Returns line `line` (1-based) without its newline, or an empty view.
  StringView Line(std::size_t line) const;
};

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SUPPORT_SOURCE_LOCATION_H_
