#ifndef SEQ_LEGACY_ASSEMBLER_OBJECT_FILE_H_
#define SEQ_LEGACY_ASSEMBLER_OBJECT_FILE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace seq_legacy {

// What the assembler hands to the linker. It exists in memory only.

struct Section {
  std::string name;
  std::vector<std::uint8_t> bytes;
};

struct Symbol {
  std::string name;
  // Index into ObjectFile::sections.
  int section = 0;
  // Offset from the start of that section.
  std::uint64_t offset = 0;
  bool global = false;
};

// A PC-relative 32-bit fix-up, the only kind there is: the four bytes at
// `offset` in `section` receive S + A - P, where S is the address of
// `symbol`, A is `addend` and P is the address of the four bytes.
struct Relocation {
  int section = 0;
  std::uint64_t offset = 0;
  std::string symbol;
  std::int64_t addend = 0;
};

struct ObjectFile {
  std::vector<Section> sections;
  std::vector<Symbol> symbols;
  std::vector<Relocation> relocations;
};

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_ASSEMBLER_OBJECT_FILE_H_
