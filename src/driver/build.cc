#include "driver/build.h"

#include <cstdint>
#include <iostream>
#include <vector>

#include "assembler/assembler.h"
#include "assembler/object_file.h"
#include "codegen/x64_codegen.h"
#include "driver/run.h"
#include "ir/ir.h"
#include "ir/lowering.h"
#include "lexer/lexer.h"
#include "linker/elf_writer.h"
#include "linker/linker.h"
#include "parser/parser.h"
#include "sema/validator.h"
#include "support/diagnostics.h"
#include "support/exit_codes.h"
#include "support/filesystem.h"
#include "support/string_view.h"
#include "support/util.h"

namespace seq_legacy {

namespace {

int Fail(int status, const std::string& message) {
  std::cerr << "seqc_legacy: error: " << message << "\n";
  return status;
}

// Writes one artifact to output/temp/ and announces it as `[tag] path`.
bool WriteArtifact(const std::string& temp_dir, const std::string& file,
                   StringView contents, const char* tag, std::string* error) {
  if (!WriteFile(JoinPath(temp_dir, file), contents, error)) return false;
  if (tag != nullptr) {
    std::cout << "[" << tag << "] output/temp/" << file << "\n";
  }
  return true;
}

}  // namespace

int BuildAndRun(const std::string& source_path, const BuildOptions& options) {
  const std::string source = AbsolutePath(source_path);
  if (FileName(source) != "main.seq" || FileName(ParentPath(source)) != "src") {
    return Fail(kExitUsage, "the source file must be <project>/src/main.seq");
  }
  const std::string project_dir = ParentPath(ParentPath(source));
  const std::string output_dir = JoinPath(project_dir, "output");
  const std::string temp_dir = JoinPath(output_dir, "temp");
  const std::string display_path = GenericPath(source_path);

  std::string raw;
  std::string error;
  if (!ReadFile(source, &raw, &error)) return Fail(kExitFilesystem, error);

  std::cout << "Seq Legacy Compiler\n\n";

  // Stages 1 to 3: lexer, parser, semantic analysis.
  FrontEnd front_end;
  Diagnostics diagnostics;
  if (!RunFrontEnd(raw, display_path, &front_end, &diagnostics)) {
    std::cout.flush();
    diagnostics.Print(std::cerr, front_end.source);
    return kExitSource;
  }
  const Workflow& workflow = front_end.workflow;
  const std::string& name = workflow.name;

  if (!CreateDirectories(temp_dir, &error)) {
    return Fail(kExitFilesystem, error);
  }

  std::cout << "[lex] " << display_path << "\n";
  if (!WriteArtifact(temp_dir, name + ".tokens", TokensToText(front_end.lines),
                     nullptr, &error)) {
    return Fail(kExitFilesystem, error);
  }
  const std::size_t steps = workflow.steps.size();
  std::cout << "[parse] 1 name, " << steps << (steps == 1 ? " step" : " steps")
            << "\n";
  if (!WriteArtifact(temp_dir, name + ".ast", AstToText(front_end.program),
                     nullptr, &error)) {
    return Fail(kExitFilesystem, error);
  }
  const std::size_t requests = workflow.RequestCount();
  std::cout << "[sema] " << requests
            << (requests == 1 ? " request" : " requests") << "\n";

  // Stage 4: intermediate representation.
  IrProgram ir;
  if (!Lower(workflow, &ir, &error)) return Fail(kExitInternal, error);
  if (!WriteArtifact(temp_dir, name + ".ir", IrToText(ir), "ir", &error)) {
    return Fail(kExitFilesystem, error);
  }

  // Stage 5: code generation.
  const std::string assembly = GenerateAssembly(ir);
  if (!WriteArtifact(temp_dir, name + ".asm", assembly, "codegen", &error)) {
    return Fail(kExitFilesystem, error);
  }

  // Stage 6: assembler. It works from the text alone.
  ObjectFile object;
  std::string listing;
  std::vector<AssemblyError> assembly_errors;
  if (!Assemble(assembly, &object, &listing, &assembly_errors)) {
    for (const AssemblyError& item : assembly_errors) {
      std::cerr << "output/temp/" << name << ".asm:" << item.line
                << ": error: " << item.message << "\n";
    }
    return Fail(kExitInternal, "the generated assembly could not be assembled");
  }
  if (!WriteArtifact(temp_dir, name + ".lst", listing, "assemble", &error)) {
    return Fail(kExitFilesystem, error);
  }

  // Stage 7: linker.
  LinkedProgram linked;
  if (!Link(object, &linked, &error)) {
    return Fail(kExitInternal, "link failed: " + error);
  }
  const std::string binary_name = name + ".bin";
  const std::string binary = JoinPath(temp_dir, binary_name);
  if (!WriteExecutable(binary, BuildElf(linked), &error)) {
    return Fail(kExitFilesystem, error);
  }
  std::cout << "[link] output/temp/" << binary_name << "\n";
  if (options.build_only) return kExitOk;

  // Stage 8: run.
  std::cout << "[run] " << binary_name << std::endl;
  int status = 0;
  if (!RunProgram(binary, output_dir, &status, &error)) {
    return Fail(kExitExecution, error);
  }
  std::cout << "[run] exit " << status << "\n";
  if (status != 0) {
    return Fail(kExitExecution,
                binary_name + " exited with status " + std::to_string(status));
  }

  std::vector<std::string> files;
  for (const IrStep& step : ir.steps) {
    for (const Op& op : step.ops) {
      if (op.kind != OpKind::kWriteFile) continue;
      // The path blob is NUL-terminated.
      const std::string file(ir.data[op.path].bytes.c_str());
      bool listed = false;
      for (const std::string& seen : files) listed = listed || seen == file;
      if (!listed) files.push_back(file);
    }
  }
  if (files.empty()) {
    std::cout << "\nResults: none\n";
  } else {
    std::cout << "\nResults:\n";
    for (const std::string& file : files) {
      std::cout << "  output/" << file << "\n";
    }
  }
  return kExitOk;
}

}  // namespace seq_legacy
