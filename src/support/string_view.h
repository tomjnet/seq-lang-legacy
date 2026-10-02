#ifndef SEQ_LEGACY_SUPPORT_STRING_VIEW_H_
#define SEQ_LEGACY_SUPPORT_STRING_VIEW_H_

#include <cstddef>
#include <cstring>
#include <ostream>
#include <string>

namespace seq_legacy {

// A view of characters that someone else owns. C++11 has no
// std::string_view; this is the part of it that seqc_legacy uses, with the
// same names and meaning. The characters must outlive the view.
class StringView {
 public:
  static const std::size_t npos = static_cast<std::size_t>(-1);

  StringView() : data_(""), size_(0) {}
  // Implicit, like std::string_view: a function that takes a StringView
  // accepts a literal or a std::string.
  StringView(const char* text) : data_(text), size_(std::strlen(text)) {}
  StringView(const std::string& text)
      : data_(text.data()), size_(text.size()) {}
  StringView(const char* data, std::size_t size) : data_(data), size_(size) {}

  const char* data() const { return data_; }
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  const char* begin() const { return data_; }
  const char* end() const { return data_ + size_; }

  char operator[](std::size_t pos) const { return data_[pos]; }
  char front() const { return data_[0]; }
  char back() const { return data_[size_ - 1]; }

  void remove_prefix(std::size_t count) {
    data_ += count;
    size_ -= count;
  }
  void remove_suffix(std::size_t count) { size_ -= count; }

  // The view from `pos` on, at most `count` characters long. A `pos` past
  // the end gives an empty view.
  StringView substr(std::size_t pos, std::size_t count = npos) const {
    if (pos > size_) pos = size_;
    const std::size_t rest = size_ - pos;
    return StringView(data_ + pos, count < rest ? count : rest);
  }

  // Each find returns the position of the match, or npos.
  std::size_t find(char c, std::size_t pos = 0) const {
    for (; pos < size_; ++pos) {
      if (data_[pos] == c) return pos;
    }
    return npos;
  }

  std::size_t find_first_of(StringView chars) const {
    for (std::size_t pos = 0; pos < size_; ++pos) {
      if (chars.find(data_[pos]) != npos) return pos;
    }
    return npos;
  }

  std::size_t find_first_not_of(StringView chars) const {
    for (std::size_t pos = 0; pos < size_; ++pos) {
      if (chars.find(data_[pos]) == npos) return pos;
    }
    return npos;
  }

  std::size_t find_last_not_of(StringView chars) const {
    for (std::size_t pos = size_; pos > 0; --pos) {
      if (chars.find(data_[pos - 1]) == npos) return pos - 1;
    }
    return npos;
  }

  explicit operator std::string() const { return std::string(data_, size_); }

 private:
  const char* data_;
  std::size_t size_;
};

inline bool operator==(StringView a, StringView b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool operator!=(StringView a, StringView b) { return !(a == b); }

inline std::ostream& operator<<(std::ostream& out, StringView text) {
  return out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

}  // namespace seq_legacy

#endif  // SEQ_LEGACY_SUPPORT_STRING_VIEW_H_
