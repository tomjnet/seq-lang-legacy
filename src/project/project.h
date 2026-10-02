#ifndef SEQ_LEGACY_PROJECT_PROJECT_H_
#define SEQ_LEGACY_PROJECT_PROJECT_H_

#include <string>

#include "support/string_view.h"

namespace seq_legacy {

// The src/main.seq that `seqc_legacy new <name>` writes.
std::string StarterSource(StringView name);

// `seqc_legacy new <name>`: creates ./<name> with src/main.seq and output/.
// Refuses a name that is not a valid project name and a destination that
// already exists. Returns the process exit status.
int CreateProject(const std::string& name);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_PROJECT_PROJECT_H_
