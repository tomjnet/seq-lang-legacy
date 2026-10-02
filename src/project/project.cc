#include "project/project.h"

#include <iostream>

#include "sema/builtins.h"
#include "sema/validator.h"
#include "support/exit_codes.h"
#include "support/filesystem.h"
#include "support/util.h"

namespace seq_legacy {

namespace {

int Fail(int status, const std::string& message) {
  std::cerr << "seqc_legacy: error: " << message << "\n";
  return status;
}

}  // namespace

std::string StarterSource(StringView name) {
  return "name = \"" + std::string(name) +
         "\"\n"
         "\n"
         "step step1():\n"
         "    ask(\"create file company.txt with 5 " +
         kSampleDatasetName +
         "\")\n"
         "\n"
         "step step2():\n"
         "    ask(\"for-each transaction get 3 top " +
         kRevenueFilterName + "\")\n";
}

int CreateProject(const std::string& name) {
  std::string reason;
  if (!IsValidProjectName(name, &reason)) {
    return Fail(kExitUsage, "invalid project name '" + name + "': " + reason);
  }
  // The project root is ./<name>.
  if (PathExists(name)) {
    return Fail(kExitFilesystem, "'" + name + "' already exists");
  }

  std::cout << "Seq Legacy Compiler\n\nCreating project: " << name << "\n\n";

  std::string error;
  // `relative` is the path below the project root, "" for the root itself.
  const auto make_directory = [&](const std::string& relative) -> bool {
    const std::string path = name + "/" + relative;
    if (!CreateDirectories(path, &error)) return false;
    std::cout << "[create] " << path << "\n";
    return true;
  };
  const auto make_source = [&]() -> bool {
    const std::string path = name + "/src/main.seq";
    if (!WriteFile(path, StarterSource(name), &error)) return false;
    std::cout << "[create] " << path << "\n";
    return true;
  };
  if (!make_directory("") || !make_directory("src/") || !make_source() ||
      !make_directory("output/")) {
    // Do not leave half a project behind.
    RemoveAll(name);
    return Fail(kExitFilesystem, error);
  }

  std::cout << "\nTarget: Linux x86_64\n"
               "\n"
               "Project created successfully.\n"
               "\n"
               "Next:\n"
               "  cd "
            << name
            << "\n"
               "  seqc_legacy src/main.seq\n";
  return kExitOk;
}

}  // namespace seq_legacy
