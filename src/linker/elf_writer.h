#ifndef SEQ_LEGACY_LINKER_ELF_WRITER_H_
#define SEQ_LEGACY_LINKER_ELF_WRITER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "linker/linker.h"

namespace seq_legacy {

constexpr std::size_t kElfHeaderSize = 64;
constexpr std::size_t kProgramHeaderSize = 56;

// Returns the bytes of a static ELF64 executable for Linux x86-64: the ELF
// header, one PT_LOAD program header that maps the whole file readable and
// executable at kBaseAddress, padding up to kImageOffset, and the image.
// There are no section headers.
std::vector<std::uint8_t> BuildElf(const LinkedProgram& program);

// Writes `bytes` to `path` with mode 0755.
bool WriteExecutable(const std::string& path,
                     const std::vector<std::uint8_t>& bytes,
                     std::string* error);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_LINKER_ELF_WRITER_H_
