#ifndef SEQ_LEGACY_ASSEMBLER_ASSEMBLER_H_
#define SEQ_LEGACY_ASSEMBLER_ASSEMBLER_H_

#include <cstddef>
#include <string>
#include <vector>

#include "assembler/object_file.h"
#include "support/string_view.h"

namespace seq_legacy {

struct AssemblyError {
  // 1-based line in the assembly text.
  std::size_t line = 0;
  std::string message;
};

// Assembles x86-64 assembly text in Intel syntax. One statement per line;
// `;` starts a comment. The statements are:
//
//   section .text | section .rodata
//   global <label>
//   <label>:
//   db <item>, ...        decimal bytes 0-255 and "printable ASCII" strings
//   mov r32, imm32        mov r64, r64        xor r32, r32
//   test r64, r64         lea r64, [rel <label>]
//   call <label>          js <label>          ret          syscall
//
// Registers are rax rcx rdx rbx rsp rbp rsi rdi and their 32-bit forms.
// Every reference to a label becomes a relocation for the linker.
//
// `listing` receives the offset, the encoded bytes and the text of each
// line. Returns false, with one entry in `errors` per bad line, if the text
// cannot be assembled.
bool Assemble(StringView text, ObjectFile* object, std::string* listing,
              std::vector<AssemblyError>* errors);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_ASSEMBLER_ASSEMBLER_H_
