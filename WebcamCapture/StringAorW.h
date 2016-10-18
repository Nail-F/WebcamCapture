#pragma once
#include "Noncopyable.h"

#include <string>

class StringAorW : Noncopyable
{
public:
  StringAorW(const char *astr, bool duplicate = false);
  StringAorW(const std::string &astr, bool duplicate = false);
  StringAorW(const wchar_t *wstr, bool duplicate = false);
  StringAorW(const std::wstring &wstr, bool duplicate = false);
  virtual ~StringAorW();
  const wchar_t * StrW(size_t size = 0) const;
  const char *    StrA(size_t size = 0) const;
  std::string     ToUTF8()const;
  bool            IsNull() const;
  bool            IsNullOrEmpty() const;
  void *          Data() const;
  operator        bool() const;
  bool            Compare(const char *astr) const;
  bool            Compare(const wchar_t *wstr) const;
  bool            Compare(const StringAorW &awstr) const;

private:
  union
  {
    const char    *string_a_;
    const wchar_t *string_w_;
  };
  union
  {
    mutable char    *string_buffer_a_;
    mutable wchar_t *string_buffer_w_;
  };
  bool is_unicode_;
  bool is_owner_;
};