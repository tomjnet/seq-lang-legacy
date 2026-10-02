#include "assembler/assembler.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <utility>

#include "support/util.h"

namespace seq_legacy {

namespace {

constexpr int kRegisterCount = 8;
constexpr const char* kRegisters64[kRegisterCount] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"};
constexpr const char* kRegisters32[kRegisterCount] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};

constexpr std::uint8_t kRexW = 0x48;
// rel32 fields are relative to the end of the field, four bytes on.
constexpr std::int64_t kRel32Addend = -4;
// Bytes shown on one listing row.
constexpr std::size_t kListingBytesPerRow = 8;

struct Operand {
  enum class Kind {
    kInvalid,
    kRegister32,
    kRegister64,
    kImmediate,
    // [rel label]
    kRelative,
    kLabel,
  };
  Kind kind = Kind::kInvalid;
  int reg = 0;
  std::uint32_t value = 0;
  std::string label;
};

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

bool IsLabelName(StringView text) {
  if (text.empty() || IsDigit(text.front())) return false;
  for (const char c : text) {
    const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                    IsDigit(c) || c == '_' || c == '.';
    if (!ok) return false;
  }
  return true;
}

// Parses a decimal number of at most `max`.
bool ParseNumber(StringView text, std::uint64_t max, std::uint64_t* out) {
  if (text.empty() || text.size() > 10) return false;
  std::uint64_t value = 0;
  for (const char c : text) {
    if (!IsDigit(c)) return false;
    value = value * 10 + static_cast<std::uint64_t>(c - '0');
  }
  if (value > max) return false;
  *out = value;
  return true;
}

Operand ParseOperand(StringView text) {
  Operand operand;
  for (int i = 0; i < kRegisterCount; ++i) {
    if (text == kRegisters64[i]) {
      operand.kind = Operand::Kind::kRegister64;
      operand.reg = i;
      return operand;
    }
    if (text == kRegisters32[i]) {
      operand.kind = Operand::Kind::kRegister32;
      operand.reg = i;
      return operand;
    }
  }
  if (!text.empty() && IsDigit(text.front())) {
    std::uint64_t value = 0;
    if (ParseNumber(text, 0xFFFFFFFFu, &value)) {
      operand.kind = Operand::Kind::kImmediate;
      operand.value = static_cast<std::uint32_t>(value);
    }
    return operand;
  }
  if (text.size() >= 2 && text.front() == '[' && text.back() == ']') {
    const StringView inner = TrimWhitespace(text.substr(1, text.size() - 2));
    if (inner.substr(0, 4) == "rel ") {
      const StringView label = TrimWhitespace(inner.substr(4));
      if (IsLabelName(label)) {
        operand.kind = Operand::Kind::kRelative;
        operand.label = std::string(label);
      }
    }
    return operand;
  }
  if (IsLabelName(text)) {
    operand.kind = Operand::Kind::kLabel;
    operand.label = std::string(text);
  }
  return operand;
}

// Removes a `;` comment. A `;` inside a quoted string is data.
StringView StripComment(StringView line) {
  bool quoted = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    if (line[i] == '"') quoted = !quoted;
    if (line[i] == ';' && !quoted) return line.substr(0, i);
  }
  return line;
}

std::uint8_t ModRm(int mod, int reg, int rm) {
  return static_cast<std::uint8_t>((mod << 6) | (reg << 3) | rm);
}

class Assembler {
 public:
  Assembler(ObjectFile* object, std::vector<AssemblyError>* errors)
      : object_(object), errors_(errors) {}

  void Line(std::size_t number, StringView raw) {
    line_ = number;
    const int section_before = section_;
    const std::size_t size_before = Size();

    const StringView text = TrimWhitespace(StripComment(raw));
    if (!text.empty()) Statement(text);

    Row row;
    row.line = number;
    row.text = std::string(raw);
    // A line that switches section emits nothing.
    if (section_ >= 0 && section_ == section_before) {
      row.section = section_;
      row.offset = size_before;
      row.size = Size() - size_before;
    }
    rows_.push_back(std::move(row));
  }

  void Finish() {
    for (const Global& global : globals_) {
      bool found = false;
      for (Symbol& symbol : object_->symbols) {
        if (symbol.name != global.name) continue;
        symbol.global = true;
        found = true;
      }
      if (!found) {
        Report(global.line,
               "global symbol '" + global.name + "' is not defined");
      }
    }
  }

  std::string Listing() const {
    std::string out;
    for (const Row& row : rows_) {
      if (row.size == 0) {
        AppendListingRow(std::to_string(row.line), "", "", row.text, &out);
        continue;
      }
      const std::vector<std::uint8_t>& bytes =
          object_->sections[static_cast<std::size_t>(row.section)].bytes;
      for (std::size_t done = 0; done < row.size; done += kListingBytesPerRow) {
        std::string hex;
        for (std::size_t i = done;
             i < row.size && i < done + kListingBytesPerRow; ++i) {
          char buffer[4];
          std::snprintf(buffer, sizeof(buffer), "%02X", bytes[row.offset + i]);
          if (!hex.empty()) hex.push_back(' ');
          hex += buffer;
        }
        char offset[16];
        std::snprintf(offset, sizeof(offset), "%08zX", row.offset + done);
        // Only the first row of a long statement repeats the line number
        // and the source text.
        AppendListingRow(done == 0 ? std::to_string(row.line) : "", offset, hex,
                         done == 0 ? row.text : "", &out);
      }
    }
    return out;
  }

 private:
  struct Row {
    std::size_t line = 0;
    int section = -1;
    std::size_t offset = 0;
    std::size_t size = 0;
    std::string text;
  };

  // A `global` statement and the line it is on.
  struct Global {
    std::string name;
    std::size_t line = 0;
  };

  static void AppendListingRow(const std::string& line,
                               const std::string& offset,
                               const std::string& hex, const std::string& text,
                               std::string* out) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%5s %-8s  %-23s  ", line.c_str(),
                  offset.c_str(), hex.c_str());
    std::string row = buffer;
    row += text;
    while (!row.empty() && row.back() == ' ') row.pop_back();
    *out += row + "\n";
  }

  std::size_t Size() const {
    if (section_ < 0) return 0;
    return object_->sections[static_cast<std::size_t>(section_)].bytes.size();
  }

  void Report(std::size_t line, std::string message) {
    AssemblyError error;
    error.line = line;
    error.message = std::move(message);
    errors_->push_back(std::move(error));
  }

  // Reports an error on the current line.
  void Fail(std::string message) { Report(line_, std::move(message)); }

  bool RequireSection(const char* what) {
    if (section_ >= 0) return true;
    Fail(std::string(what) + " outside a section");
    return false;
  }

  void Byte(std::uint8_t value) {
    object_->sections[static_cast<std::size_t>(section_)].bytes.push_back(
        value);
  }

  void Imm32(std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
      Byte(static_cast<std::uint8_t>(value >> shift));
    }
  }

  // A rel32 field that the linker fills in.
  void Reference(const std::string& label) {
    Relocation relocation;
    relocation.section = section_;
    relocation.offset = Size();
    relocation.symbol = label;
    relocation.addend = kRel32Addend;
    object_->relocations.push_back(std::move(relocation));
    Imm32(0);
  }

  void Statement(StringView text) {
    const std::size_t space = text.find_first_of(" \t");
    const StringView word = text.substr(0, space);
    const StringView rest = space == StringView::npos
                                ? StringView()
                                : TrimWhitespace(text.substr(space));

    if (word == "db") {
      if (RequireSection("data")) Data(rest);
      return;
    }
    if (text.back() == ':') {
      DefineLabel(TrimWhitespace(text.substr(0, text.size() - 1)));
      return;
    }
    if (word == "section") {
      SelectSection(rest);
      return;
    }
    if (word == "global") {
      if (!IsLabelName(rest)) {
        Fail("bad label '" + std::string(rest) + "'");
        return;
      }
      Global global;
      global.name = std::string(rest);
      global.line = line_;
      globals_.push_back(std::move(global));
      return;
    }
    if (!RequireSection("instruction")) return;

    std::vector<Operand> operands;
    std::size_t start = 0;
    while (!rest.empty() && start <= rest.size()) {
      std::size_t end = rest.find(',', start);
      if (end == StringView::npos) end = rest.size();
      const StringView item = TrimWhitespace(rest.substr(start, end - start));
      Operand operand = ParseOperand(item);
      if (operand.kind == Operand::Kind::kInvalid) {
        Fail("bad operand '" + std::string(item) + "'");
        return;
      }
      operands.push_back(std::move(operand));
      start = end + 1;
    }
    Instruction(word, operands);
  }

  void SelectSection(StringView name) {
    if (name != ".text" && name != ".rodata") {
      Fail("unknown section '" + std::string(name) + "'");
      return;
    }
    for (std::size_t i = 0; i < object_->sections.size(); ++i) {
      if (object_->sections[i].name == name) {
        section_ = static_cast<int>(i);
        return;
      }
    }
    object_->sections.push_back(Section{std::string(name), {}});
    section_ = static_cast<int>(object_->sections.size() - 1);
  }

  void DefineLabel(StringView name) {
    if (!IsLabelName(name)) {
      Fail("bad label '" + std::string(name) + "'");
      return;
    }
    if (!RequireSection("label")) return;
    for (const Symbol& symbol : object_->symbols) {
      if (symbol.name == name) {
        Fail("duplicate label '" + std::string(name) + "'");
        return;
      }
    }
    Symbol symbol;
    symbol.name = std::string(name);
    symbol.section = section_;
    symbol.offset = Size();
    object_->symbols.push_back(std::move(symbol));
  }

  void Data(StringView text) {
    std::size_t pos = 0;
    while (true) {
      while (pos < text.size() && text[pos] == ' ') ++pos;
      if (pos >= text.size()) {
        Fail("expected a byte or a string in db");
        return;
      }
      if (text[pos] == '"') {
        const std::size_t close = text.find('"', pos + 1);
        if (close == StringView::npos) {
          Fail("unterminated string in db");
          return;
        }
        for (const char c : text.substr(pos + 1, close - pos - 1)) {
          if (c < 0x20 || c > 0x7E) {
            Fail("a db string holds printable ASCII only");
            return;
          }
          Byte(static_cast<std::uint8_t>(c));
        }
        pos = close + 1;
      } else {
        std::size_t end = pos;
        while (end < text.size() && text[end] != ',' && text[end] != ' ') ++end;
        std::uint64_t value = 0;
        if (!ParseNumber(text.substr(pos, end - pos), 255, &value)) {
          Fail("bad db item '" + std::string(text.substr(pos, end - pos)) +
               "': expected a byte from 0 to 255 or a string");
          return;
        }
        Byte(static_cast<std::uint8_t>(value));
        pos = end;
      }
      while (pos < text.size() && text[pos] == ' ') ++pos;
      if (pos >= text.size()) return;
      if (text[pos] != ',') {
        Fail("expected ',' between db items");
        return;
      }
      ++pos;
    }
  }

  void Instruction(StringView mnemonic, const std::vector<Operand>& operands) {
    using Kind = Operand::Kind;
    const auto has = [&](std::initializer_list<Kind> kinds) -> bool {
      if (operands.size() != kinds.size()) return false;
      std::size_t i = 0;
      for (const Kind kind : kinds) {
        if (operands[i++].kind != kind) return false;
      }
      return true;
    };

    if (mnemonic == "ret") {
      if (!has({})) return BadOperands(mnemonic);
      Byte(0xC3);
    } else if (mnemonic == "syscall") {
      if (!has({})) return BadOperands(mnemonic);
      Byte(0x0F);
      Byte(0x05);
    } else if (mnemonic == "call") {
      if (!has({Kind::kLabel})) return BadOperands(mnemonic);
      Byte(0xE8);
      Reference(operands[0].label);
    } else if (mnemonic == "js") {
      if (!has({Kind::kLabel})) return BadOperands(mnemonic);
      Byte(0x0F);
      Byte(0x88);
      Reference(operands[0].label);
    } else if (mnemonic == "mov") {
      if (has({Kind::kRegister32, Kind::kImmediate})) {
        Byte(static_cast<std::uint8_t>(0xB8 + operands[0].reg));
        Imm32(operands[1].value);
      } else if (has({Kind::kRegister64, Kind::kRegister64})) {
        Byte(kRexW);
        Byte(0x89);
        Byte(ModRm(3, operands[1].reg, operands[0].reg));
      } else {
        return BadOperands(mnemonic);
      }
    } else if (mnemonic == "xor") {
      if (!has({Kind::kRegister32, Kind::kRegister32})) {
        return BadOperands(mnemonic);
      }
      Byte(0x31);
      Byte(ModRm(3, operands[1].reg, operands[0].reg));
    } else if (mnemonic == "test") {
      if (!has({Kind::kRegister64, Kind::kRegister64})) {
        return BadOperands(mnemonic);
      }
      Byte(kRexW);
      Byte(0x85);
      Byte(ModRm(3, operands[1].reg, operands[0].reg));
    } else if (mnemonic == "lea") {
      if (!has({Kind::kRegister64, Kind::kRelative})) {
        return BadOperands(mnemonic);
      }
      Byte(kRexW);
      Byte(0x8D);
      // mod 00 with rm 101 is RIP-relative with a 32-bit displacement.
      Byte(ModRm(0, operands[0].reg, 5));
      Reference(operands[1].label);
    } else {
      Fail("unknown mnemonic '" + std::string(mnemonic) + "'");
    }
  }

  void BadOperands(StringView mnemonic) {
    Fail("unsupported operands for '" + std::string(mnemonic) + "'");
  }

  ObjectFile* object_;
  std::vector<AssemblyError>* errors_;
  // Index of the current section, or -1 before the first `section`.
  int section_ = -1;
  std::size_t line_ = 0;
  std::vector<Global> globals_;
  std::vector<Row> rows_;
};

}  // namespace

bool Assemble(StringView text, ObjectFile* object, std::string* listing,
              std::vector<AssemblyError>* errors) {
  *object = ObjectFile();
  errors->clear();
  Assembler assembler(object, errors);

  std::size_t number = 0;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == StringView::npos) end = text.size();
    assembler.Line(++number, text.substr(start, end - start));
    start = end + 1;
  }
  assembler.Finish();
  if (listing != nullptr) *listing = assembler.Listing();
  return errors->empty();
}

}  // namespace seq_legacy
