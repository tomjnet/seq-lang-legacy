#ifndef SEQ_LEGACY_SUPPORT_FILESYSTEM_H_
#define SEQ_LEGACY_SUPPORT_FILESYSTEM_H_

#include <string>
#include <vector>

namespace seq_legacy {

// The file system operations that seqc_legacy needs. C++11 has no
// std::filesystem, so they are written against the operating system here.
//
// A path is a std::string. Its components are separated by '/'; on Windows
// '\' separates them as well.

// `path` with every separator, or run of separators, written as one '/'.
// This is the form shown in messages.
std::string GenericPath(const std::string& path);

// `base` and `name` joined by one separator.
std::string JoinPath(const std::string& base, const std::string& name);

// The last component of `path`; empty if the path ends with a separator.
std::string FileName(const std::string& path);

// `path` without its last component. The parent of a root is the root.
std::string ParentPath(const std::string& path);

// `path` made absolute against the current directory, with its "." and ".."
// components resolved by name alone: neither the path nor anything it
// passes through has to exist.
std::string AbsolutePath(const std::string& path);

// Whether `path` names an existing file or directory.
bool PathExists(const std::string& path);

// Creates the directory `path` and every missing directory above it. A
// directory that already exists is not an error.
bool CreateDirectories(const std::string& path, std::string* error);

// The names of the entries of the directory `path`, without "." and "..",
// in no particular order. Returns false if the directory cannot be read.
bool ListDirectory(const std::string& path, std::vector<std::string>* names);

// Deletes `path` and, if it is a directory, everything in it. Does nothing
// if there is no such path. Symbolic links are deleted, not followed.
void RemoveAll(const std::string& path);

// Gives the file `path` mode 0755. Windows has no such mode, so this does
// nothing there.
bool MakeExecutable(const std::string& path, std::string* error);

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SUPPORT_FILESYSTEM_H_
