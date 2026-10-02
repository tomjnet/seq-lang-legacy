#include "ir/ir.h"

#include "support/util.h"

namespace seq_legacy {

std::string IrToText(const IrProgram& program) {
  std::string out = "program " + program.name + "\n";
  for (const DataBlob& blob : program.data) {
    out += "data " + blob.name + " = ";
    if (blob.is_path) {
      // Shown without its terminator, which is spelled out as \0.
      std::string quoted = Quote(blob.bytes.substr(0, blob.bytes.size() - 1));
      quoted.insert(quoted.size() - 1, "\\0");
      out += quoted;
    } else {
      out += std::to_string(blob.bytes.size()) + " bytes";
    }
    if (!blob.comment.empty()) out += "  ; " + blob.comment;
    out += "\n";
  }
  for (const IrStep& step : program.steps) {
    out += "step " + std::to_string(step.number) + " " + step.name + ":\n";
    for (const Op& op : step.ops) {
      if (op.kind == OpKind::kWriteFile) {
        out += "  write_file " + program.data[op.path].name + ", " +
               program.data[op.data].name + "\n";
      } else {
        out += "  print " + program.data[op.data].name + "\n";
      }
    }
  }
  return out;
}

}  // namespace seq_legacy
