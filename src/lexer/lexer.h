#ifndef SEQ_LEGACY_LEXER_LEXER_H_
#define SEQ_LEGACY_LEXER_LEXER_H_

#include <cstddef>
#include <string>
#include <vector>

#include "support/diagnostics.h"
#include "support/source_location.h"
#include "support/string_view.h"

namespace seq_legacy {

// Language limits. Exceeding one is an error, never silent truncation.
constexpr std::size_t kMaxSourceBytes = 1024 * 1024;
constexpr std::size_t kMaxNameLength = 64;
constexpr std::size_t kMaxPromptBytes = 4096;
constexpr std::size_t kMaxSteps = 64;
constexpr std::size_t kMaxAsksPerStep = 16;

// The same token set as the original seqc.
enum class TokenKind {
  kIdentifier,
  kString,
  kNumber,
  kLeftParen,
  kRightParen,
  kColon,
  kEquals,
  kDot,
  kComma,
};

struct Token {
  TokenKind kind = TokenKind::kIdentifier;
  // Decoded value for a string, source spelling for everything else.
  std::string text;
  SourceLocation location;
};

// One logical line that carries tokens. Blank and comment-only lines are not
// reported: they have no indentation meaning.
struct SourceLine {
  std::size_t number = 0;
  // Count of leading spaces.
  std::size_t indent = 0;
  std::vector<Token> tokens;
  // Location just past the last token, for "expected X" messages.
  SourceLocation end;
  // A lexical error was reported on this line; the parser skips it.
  bool has_error = false;
};

// Normalizes raw file bytes: drops one leading byte-order mark, converts CRLF
// to LF, and rejects invalid UTF-8, NUL, tabs, a lone CR, and other control
// characters. Returns false if the text cannot be lexed at all.
bool NormalizeSource(StringView raw, std::string display_path, SourceText* out,
                     Diagnostics* diagnostics);

std::vector<SourceLine> Lex(const SourceText& source, Diagnostics* diagnostics);

// name, step, and ask, plus model and backend, which the original language
// uses and seqc_legacy rejects.
bool IsReservedWord(StringView word);

const char* TokenKindName(TokenKind kind);

// The <name>.tokens artifact: one line per token, grouped by source line.
std::string TokensToText(const std::vector<SourceLine>& lines);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_LEXER_LEXER_H_
