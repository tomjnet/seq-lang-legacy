#include "linker/linker.h"

#include <cstddef>
#include <limits>
#include <map>
#include <utility>

namespace seq_legacy {

namespace {

std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

}  // namespace

bool Link(const ObjectFile& object, LinkedProgram* program,
          std::string* error) {
  *program = LinkedProgram();

  // Layout: .text first, then the other sections in object order.
  std::vector<std::size_t> order;
  for (std::size_t i = 0; i < object.sections.size(); ++i) {
    if (object.sections[i].name == ".text") order.push_back(i);
  }
  for (std::size_t i = 0; i < object.sections.size(); ++i) {
    if (object.sections[i].name != ".text") order.push_back(i);
  }

  // Offset of each section in the image, indexed like object.sections.
  std::vector<std::uint64_t> section_offset(object.sections.size(), 0);
  for (const std::size_t index : order) {
    const Section& section = object.sections[index];
    const std::uint64_t offset =
        AlignUp(program->image.size(), kSectionAlignment);
    program->image.resize(offset, 0);
    program->image.insert(program->image.end(), section.bytes.begin(),
                          section.bytes.end());
    section_offset[index] = offset;
    PlacedSection placed;
    placed.name = section.name;
    placed.address = kBaseAddress + kImageOffset + offset;
    placed.size = section.bytes.size();
    program->sections.push_back(std::move(placed));
  }
  const auto section_address = [&](int section) {
    return kBaseAddress + kImageOffset +
           section_offset[static_cast<std::size_t>(section)];
  };
  const auto in_range = [&](int section) {
    return section >= 0 &&
           static_cast<std::size_t>(section) < object.sections.size();
  };

  std::map<std::string, std::uint64_t> addresses;
  for (const Symbol& symbol : object.symbols) {
    if (!in_range(symbol.section)) {
      *error = "symbol '" + symbol.name + "' is in an unknown section";
      return false;
    }
    const std::uint64_t address =
        section_address(symbol.section) + symbol.offset;
    if (!addresses.emplace(symbol.name, address).second) {
      *error = "duplicate symbol '" + symbol.name + "'";
      return false;
    }
  }

  const auto entry = addresses.find(kEntrySymbol);
  if (entry == addresses.end()) {
    *error = std::string("undefined entry symbol '") + kEntrySymbol + "'";
    return false;
  }
  program->entry = entry->second;

  for (const Relocation& relocation : object.relocations) {
    const auto target = addresses.find(relocation.symbol);
    if (target == addresses.end()) {
      *error = "undefined symbol '" + relocation.symbol + "'";
      return false;
    }
    if (!in_range(relocation.section) ||
        relocation.offset + 4 >
            object.sections[static_cast<std::size_t>(relocation.section)]
                .bytes.size()) {
      *error = "relocation against '" + relocation.symbol +
               "' lies outside its section";
      return false;
    }
    // S + A - P.
    const std::uint64_t place =
        section_address(relocation.section) + relocation.offset;
    const std::int64_t value = static_cast<std::int64_t>(target->second) +
                               relocation.addend -
                               static_cast<std::int64_t>(place);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
      *error = "relocation against '" + relocation.symbol +
               "' does not fit in 32 bits";
      return false;
    }
    const std::uint32_t bits = static_cast<std::uint32_t>(value);
    const std::size_t at =
        static_cast<std::size_t>(place - kBaseAddress - kImageOffset);
    for (std::size_t i = 0; i < 4; ++i) {
      program->image[at + i] = static_cast<std::uint8_t>(bits >> (8 * i));
    }
  }
  return true;
}

}  // namespace seq_legacy
