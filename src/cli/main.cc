#include <string>
#include <vector>

#include "cli/commands.h"

int main(int argc, char* argv[]) {
  const std::vector<std::string> arguments(argv + 1, argv + argc);
  return seq_legacy::RunCommandLine(arguments);
}
