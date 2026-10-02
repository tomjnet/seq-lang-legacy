#include "linker/elf_writer.h"

#include "support/filesystem.h"
#include "support/string_view.h"
#include "support/util.h"

namespace seq_legacy {

namespace {

// Values from the ELF specification.
constexpr std::uint8_t kElfClass64 = 2;
constexpr std::uint8_t kElfDataLittleEndian = 1;
constexpr std::uint8_t kElfVersionCurrent = 1;
constexpr std::uint16_t kElfTypeExecutable = 2;
constexpr std::uint16_t kElfMachineX86_64 = 62;
constexpr std::uint32_t kSegmentLoad = 1;
constexpr std::uint32_t kSegmentExecute = 1;
constexpr std::uint32_t kSegmentRead = 4;
constexpr std::uint64_t kPageSize = 0x1000;

class ByteWriter {
 public:
  explicit ByteWriter(std::vector<std::uint8_t>* out) : out_(out) {}

  void U8(std::uint8_t value) { out_->push_back(value); }
  void U16(std::uint16_t value) { Little(value, 2); }
  void U32(std::uint32_t value) { Little(value, 4); }
  void U64(std::uint64_t value) { Little(value, 8); }

 private:
  void Little(std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
      out_->push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }
  }

  std::vector<std::uint8_t>* out_;
};

}  // namespace

std::vector<std::uint8_t> BuildElf(const LinkedProgram& program) {
  const std::uint64_t file_size = kImageOffset + program.image.size();
  std::vector<std::uint8_t> bytes;
  bytes.reserve(static_cast<std::size_t>(file_size));
  ByteWriter out(&bytes);

  // ELF header, 64 bytes.
  out.U8(0x7F);
  out.U8('E');
  out.U8('L');
  out.U8('F');
  out.U8(kElfClass64);
  out.U8(kElfDataLittleEndian);
  out.U8(kElfVersionCurrent);
  out.U8(0);                    // OS ABI: System V
  out.U64(0);                   // ABI version and padding
  out.U16(kElfTypeExecutable);  // e_type
  out.U16(kElfMachineX86_64);   // e_machine
  out.U32(kElfVersionCurrent);  // e_version
  out.U64(program.entry);       // e_entry
  out.U64(kElfHeaderSize);      // e_phoff
  out.U64(0);                   // e_shoff
  out.U32(0);                   // e_flags
  out.U16(kElfHeaderSize);      // e_ehsize
  out.U16(kProgramHeaderSize);  // e_phentsize
  out.U16(1);                   // e_phnum
  out.U16(0);                   // e_shentsize
  out.U16(0);                   // e_shnum
  out.U16(0);                   // e_shstrndx

  // Program header, 56 bytes: one segment that maps the whole file.
  out.U32(kSegmentLoad);                    // p_type
  out.U32(kSegmentRead | kSegmentExecute);  // p_flags
  out.U64(0);                               // p_offset
  out.U64(kBaseAddress);                    // p_vaddr
  out.U64(kBaseAddress);                    // p_paddr
  out.U64(file_size);                       // p_filesz
  out.U64(file_size);                       // p_memsz
  out.U64(kPageSize);                       // p_align

  bytes.resize(static_cast<std::size_t>(kImageOffset), 0);
  bytes.insert(bytes.end(), program.image.begin(), program.image.end());
  return bytes;
}

bool WriteExecutable(const std::string& path,
                     const std::vector<std::uint8_t>& bytes,
                     std::string* error) {
  const StringView contents(reinterpret_cast<const char*>(bytes.data()),
                            bytes.size());
  return WriteFile(path, contents, error) && MakeExecutable(path, error);
}

}  // namespace seq_legacy
