#ifndef SEQ_LEGACY_LINKER_LINKER_H_
#define SEQ_LEGACY_LINKER_LINKER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "assembler/object_file.h"

namespace seq_legacy {

// The executable is loaded as one segment that maps the whole file at
// kBaseAddress. The sections start after the ELF and program headers.
constexpr std::uint64_t kBaseAddress = 0x400000;
constexpr std::uint64_t kImageOffset = 0x80;
constexpr std::uint64_t kSectionAlignment = 16;
constexpr char kEntrySymbol[] = "_start";

struct PlacedSection {
  std::string name;
  std::uint64_t address = 0;
  std::uint64_t size = 0;
};

// The linked program: section contents at their final positions, with every
// relocation applied.
struct LinkedProgram {
  // Address of `_start`.
  std::uint64_t entry = 0;
  // Bytes that follow the headers; image[0] is at file offset kImageOffset
  // and at address kBaseAddress + kImageOffset.
  std::vector<std::uint8_t> image;
  std::vector<PlacedSection> sections;
};

// Lays out the sections (.text first, each aligned to kSectionAlignment),
// gives every symbol an address and applies the relocations. Fails for an
// undefined or duplicate symbol, a missing `_start`, and a relocation that
// does not fit in 32 bits or lies outside its section.
bool Link(const ObjectFile& object, LinkedProgram* program, std::string* error);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_LINKER_LINKER_H_
