#ifndef SEQ_LEGACY_PARSER_PARSER_H_
#define SEQ_LEGACY_PARSER_PARSER_H_

#include <string>
#include <vector>

#include "lexer/lexer.h"
#include "parser/ast.h"
#include "support/diagnostics.h"

namespace seq_legacy {

// Line-driven recursive descent. It recovers at each line, so one run reports
// every syntax error in the file.
Program Parse(const std::vector<SourceLine>& lines, Diagnostics* diagnostics);

// The <name>.ast artifact.
std::string AstToText(const Program& program);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_PARSER_PARSER_H_
