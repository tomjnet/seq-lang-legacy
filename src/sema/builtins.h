#ifndef SEQ_LEGACY_SEMA_BUILTINS_H_
#define SEQ_LEGACY_SEMA_BUILTINS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "support/string_view.h"

namespace seq_legacy {

// The data and filters that are part of the language. An ask() refers to
// them by name; nothing here is written by the user.

constexpr char kSampleDatasetName[] = "sample-company-transaction-db";
constexpr char kRevenueFilterName[] = "total-revenue-by-company";

// The largest count a for-each request may ask for.
constexpr int kMaxTopCount = 100;

struct Transaction {
  std::string company;
  std::int64_t quantity = 0;
  // Unit price in cents, so that all arithmetic is integral.
  std::int64_t price_cents = 0;
};

struct Dataset {
  std::string name;
  // One "company,quantity,price" row per element, without the newline.
  std::vector<std::string> rows;
};

struct CompanyTotal {
  std::string company;
  std::int64_t cents = 0;
};

using FilterFunction =
    std::vector<CompanyTotal> (*)(const std::vector<Transaction>&);

struct Filter {
  std::string name;
  // Returns every company, best first.
  FilterFunction apply;
};

// Returns null for an unknown name.
const Dataset* FindDataset(StringView name);
const Filter* FindFilter(StringView name);

// Parses one "company,quantity,price" row. The price must have exactly two
// decimals.
bool ParseTransaction(StringView row, Transaction* out);

// Sum of quantity x price per company, highest first; companies with equal
// totals keep the order in which they first appear.
std::vector<CompanyTotal> TotalRevenueByCompany(
    const std::vector<Transaction>& transactions);

// 48000 -> "480.00".
std::string FormatCents(std::int64_t cents);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SEMA_BUILTINS_H_
