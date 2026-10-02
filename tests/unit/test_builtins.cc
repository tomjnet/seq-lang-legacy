// The built-in dataset and filter.

#include <algorithm>
#include <string>
#include <vector>

#include "sema/builtins.h"
#include "support/util.h"
#include "test_harness.h"

namespace {

using seq_legacy::CompanyTotal;
using seq_legacy::Dataset;
using seq_legacy::Transaction;

// The first `count` rows of the built-in dataset, parsed.
std::vector<Transaction> FirstRows(std::size_t count) {
  const Dataset* dataset =
      seq_legacy::FindDataset(seq_legacy::kSampleDatasetName);
  std::vector<Transaction> rows;
  for (std::size_t i = 0; i < count; ++i) {
    Transaction transaction;
    CHECK(seq_legacy::ParseTransaction(dataset->rows[i], &transaction));
    rows.push_back(transaction);
  }
  return rows;
}

Transaction MakeTransaction(const std::string& company, int quantity,
                            int price_cents) {
  Transaction transaction;
  transaction.company = company;
  transaction.quantity = quantity;
  transaction.price_cents = price_cents;
  return transaction;
}

}  // namespace

SEQ_TEST(BuiltinsAreFoundByNameOnly) {
  CHECK(seq_legacy::FindDataset("sample-company-transaction-db") != nullptr);
  CHECK(seq_legacy::FindDataset("total-revenue-by-company") == nullptr);
  CHECK(seq_legacy::FindDataset("") == nullptr);
  CHECK(seq_legacy::FindFilter("total-revenue-by-company") != nullptr);
  CHECK(seq_legacy::FindFilter("sample-company-transaction-db") == nullptr);
}

SEQ_TEST(DatasetHasFiveHundredValidRows) {
  const Dataset* dataset =
      seq_legacy::FindDataset(seq_legacy::kSampleDatasetName);
  CHECK_EQ(dataset->rows.size(), 500u);

  const std::vector<std::string> companies = {
      "ACME",  "Globex", "Initech", "Umbrella",  "Hooli",
      "Stark", "Wayne",  "Wonka",   "Cyberdyne", "Soylent"};
  for (const std::string& row : dataset->rows) {
    Transaction transaction;
    if (!seq_legacy::ParseTransaction(row, &transaction)) {
      seq_legacy_test::ReportFailure(__FILE__, __LINE__, "bad row: " + row);
      continue;
    }
    CHECK(std::find(companies.begin(), companies.end(), transaction.company) !=
          companies.end());
    CHECK(transaction.quantity >= 1 && transaction.quantity <= 50);
    CHECK(transaction.price_cents >= 500 && transaction.price_cents <= 25000);
  }
}

SEQ_TEST(DatasetStartsWithTheTenFixedRows) {
  const Dataset* dataset =
      seq_legacy::FindDataset(seq_legacy::kSampleDatasetName);
  const std::vector<std::string> fixed = {
      "ACME,10,25.50",  "Globex,4,120.00",  "Initech,30,9.75",
      "ACME,5,26.00",   "Umbrella,2,40.00", "Globex,2,118.50",
      "Hooli,12,33.25", "Initech,8,10.00",  "Umbrella,15,41.20",
      "Hooli,3,34.00"};
  for (std::size_t i = 0; i < fixed.size(); ++i) {
    CHECK_EQ(dataset->rows[i], fixed[i]);
  }
}

SEQ_TEST(EmbeddedDatasetIsTheCommittedFile) {
  std::string file;
  std::string error;
  CHECK(seq_legacy::ReadFile(SEQ_LEGACY_DATASET_FILE, &file, &error));
  // The file itself: LF line endings and a final newline.
  CHECK(file.find('\r') == std::string::npos);
  CHECK(!file.empty() && file.back() == '\n');
  CHECK_EQ(std::count(file.begin(), file.end(), '\n'), 500);

  std::string embedded;
  for (const std::string& row :
       seq_legacy::FindDataset(seq_legacy::kSampleDatasetName)->rows) {
    embedded += row + "\n";
  }
  CHECK_EQ(embedded, file);
}

SEQ_TEST(ParseTransactionRejectsMalformedRows) {
  Transaction transaction;
  CHECK(seq_legacy::ParseTransaction("ACME,10,25.50", &transaction));
  CHECK_EQ(transaction.company, std::string("ACME"));
  CHECK_EQ(transaction.quantity, 10);
  CHECK_EQ(transaction.price_cents, 2550);
  CHECK(!seq_legacy::ParseTransaction("ACME,10", &transaction));
  CHECK(!seq_legacy::ParseTransaction("ACME,10,25.5", &transaction));
  CHECK(!seq_legacy::ParseTransaction("ACME,10,25", &transaction));
  CHECK(!seq_legacy::ParseTransaction("ACME,ten,25.50", &transaction));
  CHECK(!seq_legacy::ParseTransaction(",10,25.50", &transaction));
  CHECK(!seq_legacy::ParseTransaction("ACME,10,-5.00", &transaction));
}

SEQ_TEST(FilterTotalsForFiveRows) {
  const std::vector<CompanyTotal> totals =
      seq_legacy::TotalRevenueByCompany(FirstRows(5));
  CHECK_EQ(totals.size(), 4u);
  CHECK_EQ(totals[0].company, std::string("Globex"));
  CHECK_EQ(totals[0].cents, 48000);
  CHECK_EQ(totals[1].company, std::string("ACME"));
  CHECK_EQ(totals[1].cents, 38500);
  CHECK_EQ(totals[2].company, std::string("Initech"));
  CHECK_EQ(totals[2].cents, 29250);
  CHECK_EQ(totals[3].company, std::string("Umbrella"));
  CHECK_EQ(totals[3].cents, 8000);
}

SEQ_TEST(FilterTotalsForTenRows) {
  const std::vector<CompanyTotal> totals =
      seq_legacy::TotalRevenueByCompany(FirstRows(10));
  CHECK_EQ(totals.size(), 5u);
  CHECK_EQ(totals[0].company, std::string("Globex"));
  CHECK_EQ(totals[0].cents, 71700);
  CHECK_EQ(totals[1].company, std::string("Umbrella"));
  CHECK_EQ(totals[1].cents, 69800);
  CHECK_EQ(totals[2].company, std::string("Hooli"));
  CHECK_EQ(totals[2].cents, 50100);
}

SEQ_TEST(FilterKeepsFirstAppearanceOrderOnTies) {
  const std::vector<Transaction> rows = {
      MakeTransaction("B", 1, 1000), MakeTransaction("A", 2, 500),
      MakeTransaction("C", 5, 1000), MakeTransaction("D", 1, 1000)};
  const std::vector<CompanyTotal> totals =
      seq_legacy::TotalRevenueByCompany(rows);
  CHECK_EQ(totals.size(), 4u);
  CHECK_EQ(totals[0].company, std::string("C"));
  CHECK_EQ(totals[1].company, std::string("B"));
  CHECK_EQ(totals[2].company, std::string("A"));
  CHECK_EQ(totals[3].company, std::string("D"));
}

SEQ_TEST(FilterOfNoRowsIsEmpty) {
  CHECK(seq_legacy::TotalRevenueByCompany({}).empty());
}

SEQ_TEST(FormatCentsWritesTwoDecimals) {
  CHECK_EQ(seq_legacy::FormatCents(48000), std::string("480.00"));
  CHECK_EQ(seq_legacy::FormatCents(29250), std::string("292.50"));
  CHECK_EQ(seq_legacy::FormatCents(5), std::string("0.05"));
  CHECK_EQ(seq_legacy::FormatCents(0), std::string("0.00"));
  CHECK_EQ(seq_legacy::FormatCents(123456789), std::string("1234567.89"));
}
