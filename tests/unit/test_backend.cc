// IR, code generator, assembler and linker.

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "assembler/assembler.h"
#include "assembler/object_file.h"
#include "codegen/x64_codegen.h"
#include "ir/ir.h"
#include "ir/lowering.h"
#include "linker/elf_writer.h"
#include "linker/linker.h"
#include "sema/validator.h"
#include "support/diagnostics.h"
#include "test_harness.h"

namespace {

using seq_legacy::AssemblyError;
using seq_legacy::IrProgram;
using seq_legacy::LinkedProgram;
using seq_legacy::ObjectFile;
using seq_legacy_test::ReadFixture;

using Bytes = std::vector<std::uint8_t>;

// The reference program with its two numbers replaced.
std::string Source(int rows, int top) {
  return "name = \"top3Company\"\n\nstep step1():\n"
         "    ask(\"create file company.txt with " +
         std::to_string(rows) +
         " sample-company-transaction-db\")\n\nstep step2():\n"
         "    ask(\"for-each transaction get " +
         std::to_string(top) + " top total-revenue-by-company\")\n";
}

IrProgram LowerSource(const std::string& source) {
  seq_legacy::FrontEnd front_end;
  seq_legacy::Diagnostics diagnostics;
  CHECK(seq_legacy::RunFrontEnd(source, "FILE", &front_end, &diagnostics));
  IrProgram program;
  std::string error;
  CHECK(seq_legacy::Lower(front_end.workflow, &program, &error));
  return program;
}

std::string Hex(const Bytes& bytes) {
  std::ostringstream out;
  out << std::hex << std::uppercase;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i != 0) out << ' ';
    if (bytes[i] < 16) out << '0';
    out << static_cast<int>(bytes[i]);
  }
  return out.str();
}

// Assembles one statement in .text and returns its bytes as hex text, or
// "error: <message>".
std::string Encode(const std::string& statement) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  if (!seq_legacy::Assemble("section .text\n" + statement + "\n", &object,
                            nullptr, &errors)) {
    return "error: " + errors[0].message;
  }
  return Hex(object.sections[0].bytes);
}

// The single error of an assembly text, as "<line>: <message>".
std::string AssemblyFailure(const std::string& text) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  if (seq_legacy::Assemble(text, &object, nullptr, &errors)) return "ok";
  if (errors.size() != 1) return std::to_string(errors.size()) + " errors";
  return std::to_string(errors[0].line) + ": " + errors[0].message;
}

std::uint64_t Little(const Bytes& bytes, std::size_t offset, int size) {
  std::uint64_t value = 0;
  for (int i = size - 1; i >= 0; --i) {
    value = (value << 8) | bytes[offset + static_cast<std::size_t>(i)];
  }
  return value;
}

std::vector<std::string> Lines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) lines.push_back(line);
  return lines;
}

seq_legacy::Symbol MakeSymbol(const std::string& name, std::uint64_t offset,
                              bool global) {
  seq_legacy::Symbol symbol;
  symbol.name = name;
  symbol.section = 0;
  symbol.offset = offset;
  symbol.global = global;
  return symbol;
}

constexpr char kSmallProgram[] =
    "section .text\n"
    "global _start\n"
    "_start:\n"
    "    call f\n"  // offset 0, field at 1
    "f:\n"
    "    lea rsi, [rel msg]\n"  // offset 5, field at 8
    "    js _start\n"           // offset 12, field at 14
    "    ret\n"                 // offset 18
    "section .rodata\n"
    "msg:\n"
    "    db \"hi\", 10\n";

}  // namespace

// --- IR -------------------------------------------------------------------

SEQ_TEST(IrOfTheReferenceProgramMatchesGolden) {
  const IrProgram program = LowerSource(Source(5, 3));
  CHECK_EQ(seq_legacy::IrToText(program), ReadFixture("golden/reference.ir"));
}

SEQ_TEST(LoweringEvaluatesTheResultsTable) {
  IrProgram program = LowerSource(Source(5, 3));
  CHECK_EQ(program.data.size(), 3u);
  CHECK_EQ(program.data[0].bytes, std::string("company.txt\0", 12));
  CHECK_EQ(program.data[1].bytes,
           std::string("ACME,10,25.50\nGlobex,4,120.00\nInitech,30,9.75\n"
                       "ACME,5,26.00\nUmbrella,2,40.00\n"));
  CHECK_EQ(program.data[2].bytes,
           std::string("Top 3 total-revenue-by-company:\n1. Globex 480.00\n"
                       "2. ACME 385.00\n3. Initech 292.50\n"));

  program = LowerSource(Source(5, 2));
  CHECK_EQ(program.data[2].bytes,
           std::string("Top 2 total-revenue-by-company:\n1. Globex 480.00\n"
                       "2. ACME 385.00\n"));

  program = LowerSource(Source(10, 3));
  CHECK_EQ(Lines(program.data[1].bytes).size(), 10u);
  CHECK_EQ(program.data[2].bytes,
           std::string("Top 3 total-revenue-by-company:\n1. Globex 717.00\n"
                       "2. Umbrella 698.00\n3. Hooli 501.00\n"));
}

SEQ_TEST(LoweringPrintsEveryCompanyWhenFewerThanAsked) {
  // Five rows hold four companies.
  const IrProgram program = LowerSource(Source(5, 100));
  CHECK_EQ(program.data[2].bytes,
           std::string("Top 4 total-revenue-by-company:\n1. Globex 480.00\n"
                       "2. ACME 385.00\n3. Initech 292.50\n"
                       "4. Umbrella 80.00\n"));
}

SEQ_TEST(LoweringUsesTheMostRecentFile) {
  const IrProgram program = LowerSource(
      "name = \"two\"\nstep a():\n"
      "    ask(\"create file a.txt with 10 sample-company-transaction-db\")\n"
      "    ask(\"create file b.txt with 1 sample-company-transaction-db\")\n"
      "    ask(\"for-each transaction get 3 top total-revenue-by-company\")\n");
  CHECK_EQ(program.steps.size(), 1u);
  CHECK_EQ(program.steps[0].ops.size(), 3u);
  CHECK_EQ(program.data.back().bytes,
           std::string("Top 1 total-revenue-by-company:\n1. ACME 255.00\n"));
}

// --- Code generator ---------------------------------------------------------

SEQ_TEST(AssemblyOfTheReferenceProgramMatchesGolden) {
  const IrProgram program = LowerSource(Source(5, 3));
  CHECK_EQ(seq_legacy::GenerateAssembly(program),
           ReadFixture("golden/reference.asm"));
}

SEQ_TEST(ChangingTheCountsChangesOnlyDataAndLengths) {
  const std::vector<std::string> base =
      Lines(seq_legacy::GenerateAssembly(LowerSource(Source(5, 3))));
  // Each pair is the number of rows and the number of companies.
  for (const std::pair<int, int>& counts :
       std::vector<std::pair<int, int>>{{10, 3}, {5, 2}}) {
    const std::vector<std::string> other = Lines(seq_legacy::GenerateAssembly(
        LowerSource(Source(counts.first, counts.second))));
    // Without the data statements and the length operands, the programs are
    // the same instruction for instruction.
    const auto code =
        [](const std::vector<std::string>& lines) -> std::vector<std::string> {
      std::vector<std::string> kept;
      for (const std::string& line : lines) {
        if (line.rfind("    db ", 0) == 0) continue;
        if (line.rfind("    mov edx, ", 0) == 0) continue;
        kept.push_back(line);
      }
      return kept;
    };
    CHECK(code(base) == code(other));
    CHECK(base != other);
  }
}

SEQ_TEST(GeneratedAssemblyQuotesOnlyPrintableBytes) {
  IrProgram program;
  program.name = "bytes";
  seq_legacy::DataBlob blob;
  blob.name = "d0";
  blob.bytes = std::string("a\"b;c\n\x01z\0", 9);
  program.data.push_back(blob);
  const std::string assembly = seq_legacy::GenerateAssembly(program);
  CHECK(assembly.find("    db \"a\", 34, \"b;c\", 10\n") != std::string::npos);
  CHECK(assembly.find("    db 1, \"z\", 0\n") != std::string::npos);

  // The assembler reads back exactly the bytes that went in.
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble(assembly, &object, nullptr, &errors));
  CHECK_EQ(object.sections.size(), 2u);
  CHECK_EQ(Hex(object.sections[1].bytes),
           Hex(Bytes(blob.bytes.begin(), blob.bytes.end())));
}

// --- Assembler --------------------------------------------------------------

SEQ_TEST(AssemblerEncodesEveryInstructionInTheTable) {
  CHECK_EQ(Encode("mov eax, 60"), std::string("B8 3C 00 00 00"));
  CHECK_EQ(Encode("mov edi, 70"), std::string("BF 46 00 00 00"));
  CHECK_EQ(Encode("mov esi, 577"), std::string("BE 41 02 00 00"));
  CHECK_EQ(Encode("mov edx, 420"), std::string("BA A4 01 00 00"));
  CHECK_EQ(Encode("mov ecx, 4294967295"), std::string("B9 FF FF FF FF"));
  CHECK_EQ(Encode("mov rbx, rax"), std::string("48 89 C3"));
  CHECK_EQ(Encode("mov rdi, rbx"), std::string("48 89 DF"));
  CHECK_EQ(Encode("xor edi, edi"), std::string("31 FF"));
  CHECK_EQ(Encode("xor eax, edx"), std::string("31 D0"));
  CHECK_EQ(Encode("test rax, rax"), std::string("48 85 C0"));
  CHECK_EQ(Encode("test rdi, rsi"), std::string("48 85 F7"));
  CHECK_EQ(Encode("lea rdi, [rel d0]"), std::string("48 8D 3D 00 00 00 00"));
  CHECK_EQ(Encode("lea rsi, [rel d0]"), std::string("48 8D 35 00 00 00 00"));
  CHECK_EQ(Encode("call f"), std::string("E8 00 00 00 00"));
  CHECK_EQ(Encode("js f"), std::string("0F 88 00 00 00 00"));
  CHECK_EQ(Encode("ret"), std::string("C3"));
  CHECK_EQ(Encode("syscall"), std::string("0F 05"));
}

SEQ_TEST(AssemblerIgnoresCommentsAndSpacing) {
  CHECK_EQ(Encode("   mov   eax ,  60   ; exit"),
           std::string("B8 3C 00 00 00"));
  CHECK_EQ(Encode("lea rdi, [ rel  d0 ]"), std::string("48 8D 3D 00 00 00 00"));
  CHECK_EQ(Encode("; nothing"), std::string(""));
}

SEQ_TEST(AssemblerEncodesData) {
  CHECK_EQ(Encode("db \"hi\", 10, 0, 255"), std::string("68 69 0A 00 FF"));
  CHECK_EQ(Encode("db \"a;b\"  ; comment"), std::string("61 3B 62"));
  CHECK_EQ(Encode("db 256"),
           std::string("error: bad db item '256': expected a byte from 0 to "
                       "255 or a string"));
  CHECK_EQ(Encode("db \"open"),
           std::string("error: unterminated string in db"));
  CHECK_EQ(Encode("db 1 2"),
           std::string("error: expected ',' between db items"));
  CHECK_EQ(Encode("db"), std::string("error: expected a byte or a string in "
                                     "db"));
}

SEQ_TEST(AssemblerRecordsSymbolsAndRelocations) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble(kSmallProgram, &object, nullptr, &errors));
  CHECK_EQ(object.sections.size(), 2u);
  CHECK_EQ(object.sections[0].name, std::string(".text"));
  CHECK_EQ(object.sections[0].bytes.size(), 19u);
  CHECK_EQ(object.sections[1].name, std::string(".rodata"));
  CHECK_EQ(Hex(object.sections[1].bytes), std::string("68 69 0A"));

  CHECK_EQ(object.symbols.size(), 3u);
  CHECK_EQ(object.symbols[0].name, std::string("_start"));
  CHECK(object.symbols[0].global);
  CHECK_EQ(object.symbols[1].name, std::string("f"));
  CHECK_EQ(object.symbols[1].offset, 5u);
  CHECK(!object.symbols[1].global);
  CHECK_EQ(object.symbols[2].name, std::string("msg"));
  CHECK_EQ(object.symbols[2].section, 1);
  CHECK_EQ(object.symbols[2].offset, 0u);

  CHECK_EQ(object.relocations.size(), 3u);
  CHECK_EQ(object.relocations[0].symbol, std::string("f"));
  CHECK_EQ(object.relocations[0].offset, 1u);
  CHECK_EQ(object.relocations[0].addend, -4);
  CHECK_EQ(object.relocations[1].symbol, std::string("msg"));
  CHECK_EQ(object.relocations[1].offset, 8u);
  CHECK_EQ(object.relocations[2].symbol, std::string("_start"));
  CHECK_EQ(object.relocations[2].offset, 14u);
}

SEQ_TEST(AssemblerReportsErrorsWithTheLineNumber) {
  CHECK_EQ(AssemblyFailure("section .text\n\n    push rax\n"),
           std::string("3: unknown mnemonic 'push'"));
  CHECK_EQ(AssemblyFailure("section .text\n    mov eax, r9\n"),
           std::string("2: unsupported operands for 'mov'"));
  CHECK_EQ(AssemblyFailure("section .text\n    mov eax, 4294967296\n"),
           std::string("2: bad operand '4294967296'"));
  CHECK_EQ(AssemblyFailure("section .text\n    mov rax, 1\n"),
           std::string("2: unsupported operands for 'mov'"));
  CHECK_EQ(AssemblyFailure("section .text\n    lea rdi, [d0]\n"),
           std::string("2: bad operand '[d0]'"));
  CHECK_EQ(AssemblyFailure("section .text\n    ret rax\n"),
           std::string("2: unsupported operands for 'ret'"));
  CHECK_EQ(AssemblyFailure("section .text\na:\n    ret\na:\n"),
           std::string("4: duplicate label 'a'"));
  CHECK_EQ(AssemblyFailure("section .bss\n"),
           std::string("1: unknown section '.bss'"));
  CHECK_EQ(AssemblyFailure("    ret\n"),
           std::string("1: instruction outside a section"));
  CHECK_EQ(AssemblyFailure("section .text\nglobal _start\n    ret\n"),
           std::string("2: global symbol '_start' is not defined"));
}

SEQ_TEST(AssemblerWritesAListing) {
  const IrProgram program = LowerSource(Source(5, 3));
  ObjectFile object;
  std::string listing;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble(seq_legacy::GenerateAssembly(program), &object,
                             &listing, &errors));
  const std::vector<std::string> lines = Lines(listing);
  CHECK(lines.size() > 40u);
  // Line number, offset, up to eight bytes, then the source text.
  CHECK_EQ(lines[0], "    1" + std::string(36, ' ') + "section .text");
  CHECK_EQ(lines[3], "    4 00000000  E8 00 00 00 00" + std::string(11, ' ') +
                         "    call seq_step_1");
  // A long db statement continues on rows that carry only offset and bytes.
  CHECK(listing.find("   44 00000000  63 6F 6D 70 61 6E 79 2E      "
                     "db \"company.txt\", 0\n"
                     "      00000008  74 78 74 00\n") != std::string::npos);
}

// --- Linker -----------------------------------------------------------------

SEQ_TEST(LinkerPlacesSectionsAndAppliesRelocations) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble(kSmallProgram, &object, nullptr, &errors));
  LinkedProgram linked;
  std::string error;
  CHECK(seq_legacy::Link(object, &linked, &error));

  CHECK_EQ(linked.entry, 0x400080u);
  CHECK_EQ(linked.sections.size(), 2u);
  CHECK_EQ(linked.sections[0].address, 0x400080u);
  CHECK_EQ(linked.sections[0].size, 19u);
  // .rodata is aligned to 16 after 19 bytes of .text.
  CHECK_EQ(linked.sections[1].address, 0x4000A0u);
  CHECK_EQ(linked.image.size(), 35u);

  // S + A - P: call f = 5 - 4 - 1, lea msg = 32 - 4 - 8, js _start = 0 - 4
  // - 14.
  CHECK_EQ(Little(linked.image, 1, 4), 0u);
  CHECK_EQ(Little(linked.image, 8, 4), 20u);
  CHECK_EQ(Little(linked.image, 14, 4), 0xFFFFFFEEu);
  CHECK_EQ(Hex(Bytes(linked.image.begin() + 32, linked.image.end())),
           std::string("68 69 0A"));
}

SEQ_TEST(LinkerPutsTextFirst) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(
      seq_legacy::Assemble("section .rodata\nmsg:\n    db 1, 2, 3\n"
                           "section .text\nglobal _start\n_start:\n"
                           "    lea rsi, [rel msg]\n    ret\n",
                           &object, nullptr, &errors));
  LinkedProgram linked;
  std::string error;
  CHECK(seq_legacy::Link(object, &linked, &error));
  CHECK_EQ(linked.entry, 0x400080u);
  CHECK_EQ(linked.sections[0].name, std::string(".text"));
  // msg is at image offset 16; the field is at 3.
  CHECK_EQ(Little(linked.image, 3, 4), 9u);
}

SEQ_TEST(ElfHeaderFields) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble(kSmallProgram, &object, nullptr, &errors));
  LinkedProgram linked;
  std::string error;
  CHECK(seq_legacy::Link(object, &linked, &error));
  const Bytes elf = seq_legacy::BuildElf(linked);

  CHECK_EQ(elf.size(), 0x80u + 35u);
  CHECK_EQ(Hex(Bytes(elf.begin(), elf.begin() + 8)),
           std::string("7F 45 4C 46 02 01 01 00"));
  CHECK_EQ(Little(elf, 16, 2), 2u);         // e_type: ET_EXEC
  CHECK_EQ(Little(elf, 18, 2), 62u);        // e_machine: EM_X86_64
  CHECK_EQ(Little(elf, 20, 4), 1u);         // e_version
  CHECK_EQ(Little(elf, 24, 8), 0x400080u);  // e_entry
  CHECK_EQ(Little(elf, 32, 8), 64u);        // e_phoff
  CHECK_EQ(Little(elf, 40, 8), 0u);         // e_shoff
  CHECK_EQ(Little(elf, 52, 2), 64u);        // e_ehsize
  CHECK_EQ(Little(elf, 54, 2), 56u);        // e_phentsize
  CHECK_EQ(Little(elf, 56, 2), 1u);         // e_phnum
  CHECK_EQ(Little(elf, 60, 2), 0u);         // e_shnum

  CHECK_EQ(Little(elf, 64, 4), 1u);           // p_type: PT_LOAD
  CHECK_EQ(Little(elf, 68, 4), 5u);           // p_flags: R + X
  CHECK_EQ(Little(elf, 72, 8), 0u);           // p_offset
  CHECK_EQ(Little(elf, 80, 8), 0x400000u);    // p_vaddr
  CHECK_EQ(Little(elf, 88, 8), 0x400000u);    // p_paddr
  CHECK_EQ(Little(elf, 96, 8), elf.size());   // p_filesz
  CHECK_EQ(Little(elf, 104, 8), elf.size());  // p_memsz
  CHECK_EQ(Little(elf, 112, 8), 0x1000u);     // p_align

  // The image follows the headers unchanged.
  CHECK(Bytes(elf.begin() + 0x80, elf.end()) == linked.image);
}

SEQ_TEST(LinkerReportsAnUndefinedSymbol) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble(
      "section .text\nglobal _start\n_start:\n    call missing\n", &object,
      nullptr, &errors));
  LinkedProgram linked;
  std::string error;
  CHECK(!seq_legacy::Link(object, &linked, &error));
  CHECK_EQ(error, std::string("undefined symbol 'missing'"));
}

SEQ_TEST(LinkerReportsADuplicateSymbol) {
  ObjectFile object;
  object.sections.push_back(seq_legacy::Section{".text", {0xC3}});
  object.symbols.push_back(MakeSymbol("_start", 0, true));
  object.symbols.push_back(MakeSymbol("_start", 1, false));
  LinkedProgram linked;
  std::string error;
  CHECK(!seq_legacy::Link(object, &linked, &error));
  CHECK_EQ(error, std::string("duplicate symbol '_start'"));
}

SEQ_TEST(LinkerRequiresTheEntrySymbol) {
  ObjectFile object;
  std::vector<AssemblyError> errors;
  CHECK(seq_legacy::Assemble("section .text\nmain:\n    ret\n", &object,
                             nullptr, &errors));
  LinkedProgram linked;
  std::string error;
  CHECK(!seq_legacy::Link(object, &linked, &error));
  CHECK_EQ(error, std::string("undefined entry symbol '_start'"));
}

SEQ_TEST(LinkerRejectsARelocationOutsideItsSection) {
  ObjectFile object;
  object.sections.push_back(seq_legacy::Section{".text", {0xC3}});
  object.symbols.push_back(MakeSymbol("_start", 0, true));
  seq_legacy::Relocation relocation;
  relocation.section = 0;
  relocation.offset = 0;
  relocation.symbol = "_start";
  relocation.addend = -4;
  object.relocations.push_back(relocation);
  LinkedProgram linked;
  std::string error;
  CHECK(!seq_legacy::Link(object, &linked, &error));
  CHECK_EQ(error,
           std::string("relocation against '_start' lies outside its section"));
}
