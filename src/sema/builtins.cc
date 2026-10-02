#include "sema/builtins.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace seq_legacy {

namespace {

// data/sample-company-transaction-db.txt, embedded at build time.
constexpr char kSampleDatasetText[] =
#include "sample_company_transaction_db.inc"
    ;

std::vector<std::string> SplitLines(StringView text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == StringView::npos) end = text.size();
    StringView line = text.substr(start, end - start);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (!line.empty()) lines.push_back(std::string(line));
    start = end + 1;
  }
  return lines;
}

bool ParseDigits(StringView text, std::int64_t* out) {
  if (text.empty() || text.size() > 9) return false;
  std::int64_t value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') return false;
    value = value * 10 + (c - '0');
  }
  *out = value;
  return true;
}

}  // namespace

// The built-ins are created on first use and owned here; callers receive a
// pointer that stays valid for the rest of the program.
const Dataset* FindDataset(StringView name) {
  static const std::unique_ptr<const Dataset> sample(
      new Dataset{kSampleDatasetName, SplitLines(kSampleDatasetText)});
  return name == sample->name ? sample.get() : nullptr;
}

const Filter* FindFilter(StringView name) {
  static const std::unique_ptr<const Filter> revenue(
      new Filter{kRevenueFilterName, &TotalRevenueByCompany});
  return name == revenue->name ? revenue.get() : nullptr;
}

bool ParseTransaction(StringView row, Transaction* out) {
  const std::size_t first = row.find(',');
  if (first == StringView::npos || first == 0) return false;
  const std::size_t second = row.find(',', first + 1);
  if (second == StringView::npos) return false;
  const StringView price = row.substr(second + 1);
  const std::size_t dot = price.find('.');
  if (dot == StringView::npos || price.size() - dot != 3) return false;

  std::int64_t units = 0;
  std::int64_t cents = 0;
  if (!ParseDigits(row.substr(first + 1, second - first - 1), &out->quantity) ||
      !ParseDigits(price.substr(0, dot), &units) ||
      !ParseDigits(price.substr(dot + 1), &cents)) {
    return false;
  }
  out->company = std::string(row.substr(0, first));
  out->price_cents = units * 100 + cents;
  return true;
}

std::vector<CompanyTotal> TotalRevenueByCompany(
    const std::vector<Transaction>& transactions) {
  std::vector<CompanyTotal> totals;
  for (const Transaction& transaction : transactions) {
    auto it = std::find_if(totals.begin(), totals.end(),
                           [&](const CompanyTotal& total) {
                             return total.company == transaction.company;
                           });
    if (it == totals.end()) {
      CompanyTotal total;
      total.company = transaction.company;
      totals.push_back(std::move(total));
      it = totals.end() - 1;
    }
    it->cents += transaction.quantity * transaction.price_cents;
  }
  // A stable sort keeps first-appearance order between equal totals.
  std::stable_sort(totals.begin(), totals.end(),
                   [](const CompanyTotal& a, const CompanyTotal& b) {
                     return a.cents > b.cents;
                   });
  return totals;
}

std::string FormatCents(std::int64_t cents) {
  const std::int64_t fraction = cents % 100;
  return std::to_string(cents / 100) + "." + (fraction < 10 ? "0" : "") +
         std::to_string(fraction);
}

}  // namespace seq_legacy
