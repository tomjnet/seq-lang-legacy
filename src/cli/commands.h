#ifndef SEQ_LEGACY_CLI_COMMANDS_H_
#define SEQ_LEGACY_CLI_COMMANDS_H_

#include <string>
#include <vector>

namespace seq_legacy {

// Routes the command line (without the program name) to a command and
// returns the process exit status.
int RunCommandLine(const std::vector<std::string>& arguments);

// `seqc_legacy check <file.seq>`: stages 1 to 3 only.
int CommandCheck(const std::string& source_path);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_CLI_COMMANDS_H_
