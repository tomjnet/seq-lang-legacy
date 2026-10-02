#ifndef SEQ_LEGACY_IR_IR_H_
#define SEQ_LEGACY_IR_IR_H_

#include <cstddef>
#include <string>
#include <vector>

namespace seq_legacy {

// The intermediate representation: named constant data blobs and, per step,
// a list of operations on them. It no longer mentions datasets or filters;
// those were evaluated when the workflow was lowered.

struct DataBlob {
  // Assembly label: d0, d1, ...
  std::string name;
  std::string bytes;
  // True for a NUL-terminated file path, shown as text in the .ir file.
  bool is_path = false;
  // What the bytes are, shown in the .ir file.
  std::string comment;
};

enum class OpKind {
  // Create or truncate the file `path` and write `data` to it.
  kWriteFile,
  // Write `data` to standard output.
  kPrint,
};

struct Op {
  OpKind kind = OpKind::kPrint;
  // Indexes into IrProgram::data. `path` is used by kWriteFile only.
  std::size_t path = 0;
  std::size_t data = 0;
};

struct IrStep {
  // 1-based position in the workflow.
  std::size_t number = 0;
  std::string name;
  std::vector<Op> ops;
};

struct IrProgram {
  std::string name;
  std::vector<DataBlob> data;
  std::vector<IrStep> steps;
};

// The <name>.ir artifact.
std::string IrToText(const IrProgram& program);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_IR_IR_H_
