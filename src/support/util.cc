#include "support/util.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>

#include "support/filesystem.h"

namespace seq_legacy {

namespace {

bool IsContinuation(unsigned char c) { return (c & 0xC0) == 0x80; }

}  // namespace

std::size_t Utf8SequenceLength(StringView text, std::size_t pos) {
  if (pos >= text.size()) return 0;
  const auto byte = [&](std::size_t i) {
    return static_cast<unsigned char>(text[pos + i]);
  };
  const unsigned char first = byte(0);
  if (first < 0x80) return 1;

  std::size_t length = 0;
  unsigned int code_point = 0;
  if ((first & 0xE0) == 0xC0) {
    length = 2;
    code_point = first & 0x1F;
  } else if ((first & 0xF0) == 0xE0) {
    length = 3;
    code_point = first & 0x0F;
  } else if ((first & 0xF8) == 0xF0) {
    length = 4;
    code_point = first & 0x07;
  } else {
    return 0;
  }
  if (pos + length > text.size()) return 0;
  for (std::size_t i = 1; i < length; ++i) {
    if (!IsContinuation(byte(i))) return 0;
    code_point = (code_point << 6) | (byte(i) & 0x3F);
  }
  // Reject overlong forms, surrogates, and values past U+10FFFF.
  if (length == 2 && code_point < 0x80) return 0;
  if (length == 3 && code_point < 0x800) return 0;
  if (length == 4 && (code_point < 0x10000 || code_point > 0x10FFFF)) return 0;
  if (code_point >= 0xD800 && code_point <= 0xDFFF) return 0;
  return length;
}

std::size_t Utf8Length(StringView text) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while (pos < text.size()) {
    const std::size_t length = Utf8SequenceLength(text, pos);
    pos += length == 0 ? 1 : length;
    ++count;
  }
  return count;
}

StringView TrimWhitespace(StringView text) {
  const char* const kSpace = " \t\r\n";
  const std::size_t first = text.find_first_not_of(kSpace);
  if (first == StringView::npos) return {};
  const std::size_t last = text.find_last_not_of(kSpace);
  return text.substr(first, last - first + 1);
}

std::string Quote(StringView text) {
  std::string out = "\"";
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(c);
    }
  }
  out.push_back('"');
  return out;
}

bool ReadFile(const std::string& path, std::string* out, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    *error = "cannot read '" + GenericPath(path) + "': " + std::strerror(errno);
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  if (in.bad()) {
    *error = "cannot read '" + GenericPath(path) + "'";
    return false;
  }
  *out = buffer.str();
  return true;
}

bool WriteFile(const std::string& path, StringView contents,
               std::string* error) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    *error =
        "cannot write '" + GenericPath(path) + "': " + std::strerror(errno);
    return false;
  }
  out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  out.close();
  if (!out) {
    *error = "cannot write '" + GenericPath(path) + "'";
    return false;
  }
  return true;
}

}  // namespace seq_legacy
