#ifndef SEQ_LEGACY_DRIVER_RUN_H_
#define SEQ_LEGACY_DRIVER_RUN_H_

#include <string>

namespace seq_legacy {

// Runs the executable `binary` with `working_directory` as its current
// directory and waits for it. The program shares this process's standard
// output. `exit_status` receives its exit status, or 128 plus the signal
// number if a signal ended it. Returns false if the program could not be
// started; that is always the case on a system that is not Linux.
bool RunProgram(const std::string& binary, const std::string& working_directory,
                int* exit_status, std::string* error);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_DRIVER_RUN_H_
