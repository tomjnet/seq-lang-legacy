#ifndef SEQ_LEGACY_DRIVER_BUILD_H_
#define SEQ_LEGACY_DRIVER_BUILD_H_

#include <string>

namespace seq_legacy {

struct BuildOptions {
  // Stop after the linker; do not run the program.
  bool build_only = false;
};

// `seqc_legacy <file.seq>`: runs every stage on <project>/src/main.seq,
// writes the artifacts to <project>/output/temp/, runs the program in
// <project>/output/ and lists the files it created. `source_path` is the
// path as the user wrote it. Returns the process exit status.
int BuildAndRun(const std::string& source_path, const BuildOptions& options);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_DRIVER_BUILD_H_
