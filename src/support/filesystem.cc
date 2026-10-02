#include "support/filesystem.h"

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

namespace seq_legacy {

namespace {

bool IsSeparator(char c) {
#if defined(_WIN32)
  if (c == '\\') return true;
#endif
  return c == '/';
}

// Position of the last separator in `path`, or npos.
std::size_t LastSeparator(const std::string& path) {
  for (std::size_t pos = path.size(); pos > 0; --pos) {
    if (IsSeparator(path[pos - 1])) return pos - 1;
  }
  return std::string::npos;
}

// Whether `path` is a directory or a link to one.
bool IsDirectory(const std::string& path) {
#if defined(_WIN32)
  struct _stat info;
  return _stat(path.c_str(), &info) == 0 && (info.st_mode & _S_IFDIR) != 0;
#else
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool IsSymbolicLink(const std::string& path) {
#if defined(_WIN32)
  static_cast<void>(path);
  return false;
#else
  struct stat info;
  return lstat(path.c_str(), &info) == 0 && S_ISLNK(info.st_mode);
#endif
}

bool MakeDirectory(const std::string& path) {
#if defined(_WIN32)
  return _mkdir(path.c_str()) == 0;
#else
  // rwxrwxrwx, less the process's umask.
  return mkdir(path.c_str(), 0777) == 0;
#endif
}

void RemoveEmptyDirectory(const std::string& path) {
#if defined(_WIN32)
  _rmdir(path.c_str());
#else
  rmdir(path.c_str());
#endif
}

#if !defined(_WIN32)
// Empty if the current directory cannot be determined.
std::string CurrentDirectory() {
  std::vector<char> buffer(256);
  while (getcwd(buffer.data(), buffer.size()) == nullptr) {
    if (errno != ERANGE) return "";
    buffer.resize(buffer.size() * 2);
  }
  return buffer.data();
}
#endif

}  // namespace

std::string GenericPath(const std::string& path) {
  std::string out;
  for (std::size_t i = 0; i < path.size(); ++i) {
    if (!IsSeparator(path[i])) {
      out.push_back(path[i]);
      continue;
    }
    bool repeated = i > 0 && IsSeparator(path[i - 1]);
#if defined(_WIN32)
    // "\\server\share" starts with two separators that mean something.
    if (i == 1) repeated = false;
#endif
    if (!repeated) out.push_back('/');
  }
  return out;
}

std::string JoinPath(const std::string& base, const std::string& name) {
  if (base.empty() || IsSeparator(base.back())) return base + name;
  return base + "/" + name;
}

std::string FileName(const std::string& path) {
  const std::size_t separator = LastSeparator(path);
  return separator == std::string::npos ? path : path.substr(separator + 1);
}

std::string ParentPath(const std::string& path) {
  const std::size_t separator = LastSeparator(path);
  if (separator == std::string::npos) return "";
  // The separator of a root, "/" or "C:\", is part of the root.
  bool is_root = separator == 0;
#if defined(_WIN32)
  is_root = is_root || path[separator - 1] == ':';
#endif
  return path.substr(0, is_root ? separator + 1 : separator);
}

std::string AbsolutePath(const std::string& path) {
#if defined(_WIN32)
  // _fullpath also resolves "." and ".." by name alone.
  char buffer[_MAX_PATH];
  if (_fullpath(buffer, path.c_str(), _MAX_PATH) == nullptr) return path;
  return buffer;
#else
  std::string full = path;
  if (path.empty() || path[0] != '/') {
    const std::string current = CurrentDirectory();
    if (current.empty()) return path;
    full = JoinPath(current, path);
  }

  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < full.size()) {
    std::size_t end = full.find('/', start);
    if (end == std::string::npos) end = full.size();
    const std::string part = full.substr(start, end - start);
    if (part == "..") {
      // The parent of the root is the root.
      if (!parts.empty()) parts.pop_back();
    } else if (!part.empty() && part != ".") {
      parts.push_back(part);
    }
    start = end + 1;
  }
  if (parts.empty()) return "/";
  std::string out;
  for (const std::string& part : parts) out += "/" + part;
  return out;
#endif
}

bool PathExists(const std::string& path) {
#if defined(_WIN32)
  struct _stat info;
  return _stat(path.c_str(), &info) == 0;
#else
  struct stat info;
  return stat(path.c_str(), &info) == 0;
#endif
}

bool CreateDirectories(const std::string& path, std::string* error) {
  // Each prefix of the path that ends just before a separator, and then the
  // path itself, from the top down.
  for (std::size_t end = 1; end <= path.size(); ++end) {
    if (end < path.size() && !IsSeparator(path[end])) continue;
    // A root, a doubled separator or a trailing one names nothing new.
    if (IsSeparator(path[end - 1])) continue;
#if defined(_WIN32)
    if (path[end - 1] == ':') continue;  // A drive, as in "C:\".
#endif
    const std::string prefix = path.substr(0, end);
    if (IsDirectory(prefix)) continue;
    // Something that is not a directory may be in the way.
    if (PathExists(prefix)) {
      errno = ENOTDIR;
    } else if (MakeDirectory(prefix)) {
      continue;
    }
    *error =
        "cannot create '" + GenericPath(path) + "': " + std::strerror(errno);
    return false;
  }
  return true;
}

bool ListDirectory(const std::string& path, std::vector<std::string>* names) {
  names->clear();
#if defined(_WIN32)
  struct _finddata_t entry;
  const std::intptr_t handle = _findfirst(JoinPath(path, "*").c_str(), &entry);
  if (handle == -1) return false;
  do {
    const std::string name = entry.name;
    if (name != "." && name != "..") names->push_back(name);
  } while (_findnext(handle, &entry) == 0);
  _findclose(handle);
#else
  // Closed when `directory` goes out of scope.
  const std::unique_ptr<DIR, int (*)(DIR*)> directory(opendir(path.c_str()),
                                                      &closedir);
  if (directory == nullptr) return false;
  while (const dirent* entry = readdir(directory.get())) {
    const std::string name = entry->d_name;
    if (name != "." && name != "..") names->push_back(name);
  }
#endif
  return true;
}

void RemoveAll(const std::string& path) {
  if (IsSymbolicLink(path) || !IsDirectory(path)) {
    std::remove(path.c_str());
    return;
  }
  std::vector<std::string> names;
  if (ListDirectory(path, &names)) {
    for (const std::string& name : names) RemoveAll(JoinPath(path, name));
  }
  RemoveEmptyDirectory(path);
}

bool MakeExecutable(const std::string& path, std::string* error) {
#if defined(_WIN32)
  static_cast<void>(path);
  static_cast<void>(error);
  return true;
#else
  // rwxr-xr-x.
  if (chmod(path.c_str(), 0755) == 0) return true;
  *error = "cannot make '" + GenericPath(path) +
           "' executable: " + std::strerror(errno);
  return false;
#endif
}

}  // namespace seq_legacy
