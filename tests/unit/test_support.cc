// StringView and the file system helpers, which stand in for the
// std::string_view and std::filesystem that C++11 does not have.

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "support/filesystem.h"
#include "support/string_view.h"
#include "support/util.h"
#include "test_harness.h"

namespace {

using seq_legacy::StringView;

// A directory for the tests below, under the directory the tests run in.
constexpr char kScratch[] = "seq_legacy_test_scratch";

}  // namespace

// --- StringView -------------------------------------------------------------

SEQ_TEST(StringViewIsBuiltFromLiteralsStringsAndBytes) {
  CHECK(StringView().empty());
  CHECK_EQ(StringView("abc").size(), 3u);

  const std::string text = "hello";
  const StringView view = text;
  CHECK_EQ(view.size(), 5u);
  CHECK(view.data() == text.data());
  CHECK_EQ(view.front(), 'h');
  CHECK_EQ(view.back(), 'o');
  CHECK_EQ(view[1], 'e');

  // An explicit length keeps a NUL byte.
  const StringView bytes("a\0b", 3);
  CHECK_EQ(bytes.size(), 3u);
  CHECK_EQ(std::string(bytes), std::string("a\0b", 3));
}

SEQ_TEST(StringViewComparesByContents) {
  const std::string text = "step";
  CHECK(StringView("step") == text);
  CHECK(text == StringView("step"));
  CHECK(StringView(text) == "step");
  CHECK(StringView(text) != "steps");
  CHECK(StringView(text) != "stop");
  CHECK(StringView() == "");
  CHECK(StringView("a\0b", 3) != StringView("a\0c", 3));
}

SEQ_TEST(StringViewSubstrStaysInsideTheView) {
  const StringView view = "0123456789";
  CHECK(view.substr(2, 3) == "234");
  CHECK(view.substr(7) == "789");
  CHECK(view.substr(7, 100) == "789");
  CHECK(view.substr(0, 0).empty());
  CHECK(view.substr(10).empty());
  CHECK(view.substr(11, 2).empty());
}

SEQ_TEST(StringViewRemovesPrefixAndSuffix) {
  StringView view = "[rel d0]";
  view.remove_prefix(1);
  view.remove_suffix(1);
  CHECK(view == "rel d0");
}

SEQ_TEST(StringViewFinds) {
  const StringView view = "  mov eax, 60  ";
  const bool not_found = view.find('!') == StringView::npos;
  CHECK(not_found);
  CHECK_EQ(view.find('a'), 7u);
  CHECK_EQ(view.find(' ', 2), 5u);
  CHECK_EQ(view.find_first_of(",;"), 9u);
  CHECK_EQ(view.find_first_not_of(" "), 2u);
  CHECK_EQ(view.find_last_not_of(" "), 12u);

  const bool blank_has_no_text =
      StringView("   ").find_first_not_of(" ") == StringView::npos &&
      StringView("   ").find_last_not_of(" ") == StringView::npos &&
      StringView("abc").find_first_of("xyz") == StringView::npos &&
      StringView("abc").find('a', 1) == StringView::npos;
  CHECK(blank_has_no_text);
}

SEQ_TEST(StringViewIsIterableAndPrintable) {
  std::string copy;
  for (const char c : StringView("a\0b", 3)) copy.push_back(c);
  CHECK_EQ(copy, std::string("a\0b", 3));

  std::ostringstream out;
  out << "<" << StringView("0123456789").substr(3, 4) << ">";
  CHECK_EQ(out.str(), std::string("<3456>"));
}

// --- Paths ------------------------------------------------------------------

SEQ_TEST(PathsAreJoinedAndSplitAtSeparators) {
  CHECK_EQ(seq_legacy::JoinPath("a", "b.txt"), std::string("a/b.txt"));
  CHECK_EQ(seq_legacy::JoinPath("a/", "b.txt"), std::string("a/b.txt"));
  CHECK_EQ(seq_legacy::JoinPath("", "b.txt"), std::string("b.txt"));

  CHECK_EQ(seq_legacy::FileName("p/src/main.seq"), std::string("main.seq"));
  CHECK_EQ(seq_legacy::FileName("main.seq"), std::string("main.seq"));
  CHECK_EQ(seq_legacy::FileName("p/src/"), std::string(""));

  CHECK_EQ(seq_legacy::ParentPath("p/src/main.seq"), std::string("p/src"));
  CHECK_EQ(seq_legacy::ParentPath("p/src"), std::string("p"));
  CHECK_EQ(seq_legacy::ParentPath("main.seq"), std::string(""));
  CHECK_EQ(seq_legacy::ParentPath("/p"), std::string("/"));
  CHECK_EQ(seq_legacy::ParentPath("/"), std::string("/"));

  CHECK_EQ(seq_legacy::GenericPath("p/src/main.seq"),
           std::string("p/src/main.seq"));
  CHECK_EQ(seq_legacy::GenericPath("p//src///main.seq"),
           std::string("p/src/main.seq"));
}

SEQ_TEST(AbsolutePathResolvesDotsByNameAlone) {
  const std::string source = seq_legacy::AbsolutePath("p/src/main.seq");
  CHECK(source.size() > std::string("p/src/main.seq").size());
  CHECK_EQ(seq_legacy::FileName(source), std::string("main.seq"));
  CHECK_EQ(seq_legacy::FileName(seq_legacy::ParentPath(source)),
           std::string("src"));

  // None of these directories exists.
  CHECK_EQ(seq_legacy::AbsolutePath("p/src/missing/../main.seq"), source);
  CHECK_EQ(seq_legacy::AbsolutePath("./p/./src//main.seq"), source);
  CHECK_EQ(seq_legacy::AbsolutePath("q/../p/src/main.seq"), source);
  // An absolute path stays where it is.
  CHECK_EQ(seq_legacy::AbsolutePath(source), source);
  CHECK_EQ(seq_legacy::AbsolutePath("."),
           seq_legacy::ParentPath(seq_legacy::AbsolutePath("p")));
}

// --- Directories ------------------------------------------------------------

SEQ_TEST(DirectoriesAreCreatedListedAndRemoved) {
  const std::string root = kScratch;
  seq_legacy::RemoveAll(root);
  CHECK(!seq_legacy::PathExists(root));

  std::string error;
  const std::string nested = root + "/output/temp";
  CHECK(seq_legacy::CreateDirectories(nested, &error));
  CHECK(seq_legacy::PathExists(nested));
  // Creating it again, with or without a trailing separator, is not an error.
  CHECK(seq_legacy::CreateDirectories(nested, &error));
  CHECK(seq_legacy::CreateDirectories(nested + "/", &error));
  CHECK_EQ(error, std::string(""));

  const std::string file = root + "/output/company.txt";
  CHECK(seq_legacy::WriteFile(file, "ACME,10,25.50\n", &error));
  CHECK(seq_legacy::MakeExecutable(file, &error));
  std::string contents;
  CHECK(seq_legacy::ReadFile(file, &contents, &error));
  CHECK_EQ(contents, std::string("ACME,10,25.50\n"));

  std::vector<std::string> names;
  CHECK(seq_legacy::ListDirectory(root + "/output", &names));
  std::sort(names.begin(), names.end());
  const std::vector<std::string> expected = {"company.txt", "temp"};
  CHECK(names == expected);
  CHECK(seq_legacy::ListDirectory(nested, &names));
  CHECK(names.empty());
  CHECK(!seq_legacy::ListDirectory(root + "/missing", &names));

  // A file is in the way of the directory.
  CHECK(!seq_legacy::CreateDirectories(file + "/below", &error));
  CHECK_EQ(error.rfind("cannot create '" + file + "/below': ", 0), 0u);
  CHECK(!seq_legacy::CreateDirectories(file, &error));
  CHECK(!seq_legacy::PathExists(file + "/below"));

  seq_legacy::RemoveAll(root);
  CHECK(!seq_legacy::PathExists(root));
  // Removing what is not there does nothing.
  seq_legacy::RemoveAll(root);
}
