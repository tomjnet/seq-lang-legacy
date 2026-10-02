#ifndef SEQ_LEGACY_IR_LOWERING_H_
#define SEQ_LEGACY_IR_LOWERING_H_

#include <string>

#include "ir/ir.h"
#include "sema/validator.h"

namespace seq_legacy {

// Lowers a validated workflow to IR and evaluates it as far as possible:
// a create-file request becomes the bytes of the first N dataset rows, and a
// for-each request becomes the finished text of the filter result. Nothing
// is left for the generated program to compute.
//
// Returns false only for an internal inconsistency, such as a malformed row
// in a built-in dataset.
bool Lower(const Workflow& workflow, IrProgram* program, std::string* error);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_IR_LOWERING_H_
