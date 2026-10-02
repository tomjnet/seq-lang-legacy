#include "driver/run.h"

#if defined(__linux__)

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "support/filesystem.h"

namespace seq_legacy {

namespace {

// Exit status of the child when it cannot change directory or exec. The
// generated program itself only ever exits 0 or 70.
constexpr int kChildSetupFailed = 127;

}  // namespace

bool RunProgram(const std::string& binary, const std::string& working_directory,
                int* exit_status, std::string* error) {
  // The child changes directory before it starts the program.
  const std::string program = AbsolutePath(binary);

  const pid_t child = fork();
  if (child < 0) {
    *error = std::string("cannot start the program: ") + std::strerror(errno);
    return false;
  }
  if (child == 0) {
    char* const arguments[] = {const_cast<char*>(program.c_str()), nullptr};
    if (chdir(working_directory.c_str()) == 0) {
      execv(program.c_str(), arguments);
    }
    _exit(kChildSetupFailed);
  }

  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno == EINTR) continue;
    *error =
        std::string("cannot wait for the program: ") + std::strerror(errno);
    return false;
  }
  if (WIFEXITED(status)) {
    *exit_status = WEXITSTATUS(status);
    if (*exit_status == kChildSetupFailed) {
      *error = "cannot execute '" + program + "'";
      return false;
    }
    return true;
  }
  *exit_status = 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
  return true;
}

}  // namespace seq_legacy

#else  // !defined(__linux__)

namespace seq_legacy {

bool RunProgram(const std::string&, const std::string&, int*,
                std::string* error) {
  *error =
      "the generated program is a Linux x86_64 executable and can only "
      "be run on Linux; use --build-only on this system";
  return false;
}

}  // namespace seq_legacy

#endif  // defined(__linux__)
