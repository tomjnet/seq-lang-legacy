#ifndef SEQ_LEGACY_SUPPORT_UTIL_H_
#define SEQ_LEGACY_SUPPORT_UTIL_H_

#include <cstddef>
#include <string>

#include "support/string_view.h"

namespace seq_legacy {

// Length in bytes of the valid UTF-8 sequence that starts at `pos`, or 0 if
// the bytes there are not valid UTF-8.
std::size_t Utf8SequenceLength(StringView text, std::size_t pos);

// Number of code points in `text`; an invalid byte counts as one.
std::size_t Utf8Length(StringView text);

StringView TrimWhitespace(StringView text);

// Returns `text` in double quotes, with ", \, newline and tab escaped the way
// a .seq string literal writes them.
std::string Quote(StringView text);

bool ReadFile(const std::string& path, std::string* out, std::string* error);

bool WriteFile(const std::string& path, StringView contents,
               std::string* error);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SUPPORT_UTIL_H_
