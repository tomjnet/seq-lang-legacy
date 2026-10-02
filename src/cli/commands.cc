#include "cli/commands.h"

#include <cstddef>
#include <iostream>

#include "driver/build.h"
#include "project/project.h"
#include "sema/validator.h"
#include "support/diagnostics.h"
#include "support/exit_codes.h"
#include "support/filesystem.h"
#include "support/string_view.h"
#include "support/util.h"

namespace seq_legacy {

namespace {

constexpr char kUsage[] =
    "Seq Legacy Compiler\n"
    "\n"
    "Usage:\n"
    "  seqc_legacy new <name>         Create a project in ./<name>.\n"
    "  seqc_legacy <file.seq>         Check, build, run, and list the "
    "results.\n"
    "  seqc_legacy check <file.seq>   Parse and validate only.\n"
    "  seqc_legacy --version          Show version information.\n"
    "  seqc_legacy --help             Show this message.\n"
    "\n"
    "Options for seqc_legacy <file.seq>:\n"
    "  --build-only   Stop after the linker; do not run the program.\n"
    "\n"
    "The program is compiled to a static Linux x86_64 executable by\n"
    "seqc_legacy alone: no compiler, assembler or linker is started.\n";

int UsageError(const std::string& message) {
  std::cerr << "seqc_legacy: error: " << message << "\n"
            << "Run 'seqc_legacy --help' for usage.\n";
  return kExitUsage;
}

bool IsSeqFile(StringView argument) {
  const StringView suffix = ".seq";
  return argument.size() > suffix.size() &&
         argument.substr(argument.size() - suffix.size()) == suffix;
}

}  // namespace

int CommandCheck(const std::string& source_path) {
  std::string raw;
  std::string error;
  if (!ReadFile(source_path, &raw, &error)) {
    std::cerr << "seqc_legacy: error: " << error << "\n";
    return kExitFilesystem;
  }
  FrontEnd front_end;
  Diagnostics diagnostics;
  if (!RunFrontEnd(raw, GenericPath(source_path), &front_end, &diagnostics)) {
    diagnostics.Print(std::cerr, front_end.source);
    return kExitSource;
  }
  const Workflow& workflow = front_end.workflow;
  const std::size_t requests = workflow.RequestCount();
  std::cout << front_end.source.path << ": ok\n"
            << "  name:  " << workflow.name << "\n"
            << "  steps: " << workflow.steps.size() << " (" << requests
            << (requests == 1 ? " request" : " requests") << ")\n";
  for (std::size_t i = 0; i < workflow.steps.size(); ++i) {
    std::cout << "    " << i + 1 << ". " << workflow.steps[i].name << " ("
              << workflow.steps[i].requests.size() << ")\n";
  }
  return kExitOk;
}

int RunCommandLine(const std::vector<std::string>& arguments) {
  BuildOptions options;
  std::vector<std::string> positional;
  for (const std::string& argument : arguments) {
    if (argument == "--help" || argument == "-h") {
      std::cout << kUsage;
      return kExitOk;
    }
    if (argument == "--version") {
      std::cout << "seqc_legacy " << SEQ_LEGACY_VERSION << "\n"
                << "target: Linux x86_64\n";
      return kExitOk;
    }
    if (argument == "--build-only") {
      options.build_only = true;
    } else if (!argument.empty() && argument.front() == '-') {
      return UsageError("unknown option '" + argument + "'");
    } else {
      positional.push_back(argument);
    }
  }

  if (positional.empty()) {
    std::cerr << kUsage;
    return kExitUsage;
  }
  const std::string& command = positional[0];
  if (command == "new") {
    if (positional.size() != 2) {
      return UsageError("usage: seqc_legacy new <name>");
    }
    return CreateProject(positional[1]);
  }
  if (command == "check") {
    if (positional.size() != 2 || !IsSeqFile(positional[1])) {
      return UsageError("usage: seqc_legacy check <file.seq>");
    }
    return CommandCheck(positional[1]);
  }
  if (IsSeqFile(command)) {
    if (positional.size() != 1) {
      return UsageError("unexpected argument '" + positional[1] + "'");
    }
    return BuildAndRun(command, options);
  }
  return UsageError("'" + command + "' is not a command or a .seq source file");
}

}  // namespace seq_legacy
