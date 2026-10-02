#include "ir/lowering.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "sema/builtins.h"
#include "sema/request.h"

namespace seq_legacy {

namespace {

std::size_t AddBlob(IrProgram* program, std::string bytes, bool is_path,
                    std::string comment) {
  DataBlob blob;
  blob.name = "d" + std::to_string(program->data.size());
  blob.bytes = std::move(bytes);
  blob.is_path = is_path;
  blob.comment = std::move(comment);
  program->data.push_back(std::move(blob));
  return program->data.size() - 1;
}

}  // namespace

bool Lower(const Workflow& workflow, IrProgram* program, std::string* error) {
  *program = IrProgram();
  program->name = workflow.name;

  // The rows written by the most recent create-file request: the input of a
  // later for-each request.
  std::vector<Transaction> transactions;

  for (std::size_t i = 0; i < workflow.steps.size(); ++i) {
    IrStep step;
    step.number = i + 1;
    step.name = workflow.steps[i].name;

    for (const Request& request : workflow.steps[i].requests) {
      Op op;
      if (request.kind == RequestKind::kCreateFile) {
        const Dataset* dataset = FindDataset(request.builtin);
        const std::size_t count = static_cast<std::size_t>(request.count);
        if (dataset == nullptr || count > dataset->rows.size()) {
          *error = "request was not validated: " + request.builtin;
          return false;
        }
        std::string bytes;
        transactions.clear();
        for (std::size_t row = 0; row < count; ++row) {
          Transaction transaction;
          if (!ParseTransaction(dataset->rows[row], &transaction)) {
            *error = "malformed row " + std::to_string(row + 1) + " in " +
                     dataset->name;
            return false;
          }
          transactions.push_back(std::move(transaction));
          bytes += dataset->rows[row] + "\n";
        }
        op.kind = OpKind::kWriteFile;
        op.path =
            AddBlob(program, request.file + std::string(1, '\0'), true, "");
        op.data =
            AddBlob(program, std::move(bytes), false,
                    "rows 1-" + std::to_string(count) + " of " + dataset->name);
      } else {
        const Filter* filter = FindFilter(request.builtin);
        if (filter == nullptr) {
          *error = "request was not validated: " + request.builtin;
          return false;
        }
        const std::vector<CompanyTotal> totals = filter->apply(transactions);
        const std::size_t shown =
            std::min(static_cast<std::size_t>(request.count), totals.size());
        std::string text =
            "Top " + std::to_string(shown) + " " + filter->name + ":\n";
        for (std::size_t rank = 0; rank < shown; ++rank) {
          text += std::to_string(rank + 1) + ". " + totals[rank].company + " " +
                  FormatCents(totals[rank].cents) + "\n";
        }
        op.kind = OpKind::kPrint;
        op.data =
            AddBlob(program, std::move(text), false,
                    "top " + std::to_string(shown) + " of " + filter->name);
      }
      step.ops.push_back(op);
    }
    program->steps.push_back(std::move(step));
  }
  return true;
}

}  // namespace seq_legacy
