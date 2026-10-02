#ifndef SEQ_LEGACY_SUPPORT_EXIT_CODES_H_
#define SEQ_LEGACY_SUPPORT_EXIT_CODES_H_

namespace seq_legacy {

// Process exit statuses of seqc_legacy. The numbers match the original seqc
// where the meaning is the same.
constexpr int kExitOk = 0;
constexpr int kExitInternal = 1;
constexpr int kExitUsage = 2;
constexpr int kExitSource = 3;
constexpr int kExitExecution = 6;
constexpr int kExitFilesystem = 7;

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SUPPORT_EXIT_CODES_H_
