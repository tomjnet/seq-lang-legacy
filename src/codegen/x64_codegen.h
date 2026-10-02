#ifndef SEQ_LEGACY_CODEGEN_X64_CODEGEN_H_
#define SEQ_LEGACY_CODEGEN_X64_CODEGEN_H_

#include <cstddef>
#include <string>

#include "ir/ir.h"

namespace seq_legacy {

// Label of the function generated for step `number` (1-based).
std::string StepLabel(std::size_t number);

// Turns the IR into x86-64 assembly text for Linux, in the dialect that
// assembler/assembler.h reads. The program is `_start`, which calls one
// function per step and exits 0, and `seq_fail`, which exits 70. Every step
// talks to the kernel directly with system calls.
std::string GenerateAssembly(const IrProgram& program);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_CODEGEN_X64_CODEGEN_H_
