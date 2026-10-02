#include "codegen/x64_codegen.h"

#include <cstddef>
#include <utility>

#include "support/string_view.h"

namespace seq_legacy {

namespace {

// Linux x86-64 system call numbers.
constexpr int kSysWrite = 1;
constexpr int kSysOpen = 2;
constexpr int kSysClose = 3;
constexpr int kSysExit = 60;

// O_WRONLY | O_CREAT | O_TRUNC.
constexpr int kOpenFlags = 0x1 | 0x40 | 0x200;
// rw-r--r--.
constexpr int kFileMode = 0644;
constexpr int kStdout = 1;
constexpr int kFailureStatus = 70;

// Column at which a trailing comment starts, counted from the mnemonic.
constexpr std::size_t kCommentColumn = 23;

class Emitter {
 public:
  void Raw(StringView line) {
    text_.append(line.data(), line.size());
    text_.push_back('\n');
  }

  void Label(StringView name) {
    text_.append(name.data(), name.size());
    text_.append(":\n");
  }

  void Instruction(std::string instruction, StringView comment = "") {
    text_.append("    ");
    if (!comment.empty()) {
      if (instruction.size() < kCommentColumn) {
        instruction.resize(kCommentColumn, ' ');
      } else {
        instruction.push_back(' ');
      }
      instruction.append("; ");
      instruction.append(comment.data(), comment.size());
    }
    text_.append(instruction);
    text_.push_back('\n');
  }

  void MoveImmediate(const char* reg, int value, StringView comment = "") {
    Instruction(std::string("mov ") + reg + ", " + std::to_string(value),
                comment);
  }

  // syscall, then leave through seq_fail if the kernel returned an error.
  void CheckedSyscall() {
    Instruction("syscall");
    Instruction("test rax, rax");
    Instruction("js seq_fail");
  }

  std::string Take() { return std::move(text_); }

 private:
  std::string text_;
};

bool IsQuotable(char c) { return c >= 0x20 && c < 0x7F && c != '"'; }

// Emits `bytes` as db statements: printable runs as quoted strings, every
// other byte as a decimal number. A statement ends after each newline byte,
// so the text reads line by line.
void EmitData(StringView bytes, Emitter* out) {
  std::string items;
  std::string run;
  const auto flush_run = [&]() {
    if (run.empty()) return;
    if (!items.empty()) items += ", ";
    items += "\"" + run + "\"";
    run.clear();
  };
  const auto flush_statement = [&]() {
    flush_run();
    if (items.empty()) return;
    out->Instruction("db " + items);
    items.clear();
  };

  for (const char c : bytes) {
    if (IsQuotable(c)) {
      run.push_back(c);
      continue;
    }
    flush_run();
    if (!items.empty()) items += ", ";
    items += std::to_string(static_cast<unsigned char>(c));
    if (c == '\n') flush_statement();
  }
  flush_statement();
}

void EmitWriteFile(const IrProgram& program, const Op& op, Emitter* out) {
  const DataBlob& path = program.data[op.path];
  const DataBlob& data = program.data[op.data];
  out->Instruction("lea rdi, [rel " + path.name + "]");
  out->MoveImmediate("esi", kOpenFlags, "O_WRONLY | O_CREAT | O_TRUNC");
  out->MoveImmediate("edx", kFileMode, "0644");
  out->MoveImmediate("eax", kSysOpen, "open");
  out->CheckedSyscall();
  out->Instruction("mov rbx, rax");
  out->Instruction("mov rdi, rbx");
  out->Instruction("lea rsi, [rel " + data.name + "]");
  out->MoveImmediate("edx", static_cast<int>(data.bytes.size()));
  out->MoveImmediate("eax", kSysWrite, "write");
  out->CheckedSyscall();
  out->Instruction("mov rdi, rbx");
  out->MoveImmediate("eax", kSysClose, "close");
  out->Instruction("syscall");
}

void EmitPrint(const IrProgram& program, const Op& op, Emitter* out) {
  const DataBlob& data = program.data[op.data];
  out->MoveImmediate("edi", kStdout);
  out->Instruction("lea rsi, [rel " + data.name + "]");
  out->MoveImmediate("edx", static_cast<int>(data.bytes.size()));
  out->MoveImmediate("eax", kSysWrite, "write");
  out->CheckedSyscall();
}

}  // namespace

std::string StepLabel(std::size_t number) {
  return "seq_step_" + std::to_string(number);
}

std::string GenerateAssembly(const IrProgram& program) {
  Emitter out;
  out.Raw("section .text");
  out.Raw("global _start");

  out.Label("_start");
  for (const IrStep& step : program.steps) {
    out.Instruction("call " + StepLabel(step.number));
  }
  out.Instruction("xor edi, edi");
  out.MoveImmediate("eax", kSysExit, "exit(0)");
  out.Instruction("syscall");

  out.Label("seq_fail");
  out.MoveImmediate("edi", kFailureStatus);
  out.MoveImmediate("eax", kSysExit, "exit(70)");
  out.Instruction("syscall");

  for (const IrStep& step : program.steps) {
    out.Label(StepLabel(step.number));
    for (const Op& op : step.ops) {
      if (op.kind == OpKind::kWriteFile) {
        EmitWriteFile(program, op, &out);
      } else {
        EmitPrint(program, op, &out);
      }
    }
    out.Instruction("ret");
  }

  out.Raw("section .rodata");
  for (const DataBlob& blob : program.data) {
    out.Label(blob.name);
    EmitData(blob.bytes, &out);
  }
  return out.Take();
}

}  // namespace seq_legacy
